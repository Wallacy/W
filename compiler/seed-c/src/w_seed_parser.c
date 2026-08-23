#include "w_seed_parser.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  bool right_associative;
  int precedence;
} binary_info;

static w_seed_span empty_span(size_t offset) {
  const w_seed_span span = {offset, offset};
  return span;
}

static bool source_ready(const w_seed_source *source) {
  return source != NULL &&
         (source->bytes.length == 0 || source->bytes.data != NULL);
}

static bool span_text(const w_seed_parser *parser, w_seed_span span,
                      const char *text) {
  const size_t length = strlen(text);
  if (span.end_byte < span.start_byte ||
      span.end_byte - span.start_byte != length ||
      span.end_byte > parser->source->bytes.length) {
    return false;
  }
  if (length == 0) return true;
  return memcmp(parser->source->bytes.data + span.start_byte, text, length) == 0;
}

static bool item_is_trivia(const w_seed_lex_item *item) {
  return item->kind == W_SEED_LEX_ITEM_SOURCE_PREFIX ||
         item->kind == W_SEED_LEX_ITEM_TRIVIA;
}

static w_seed_cst_kind leaf_kind(w_seed_lex_item_kind kind) {
  switch (kind) {
    case W_SEED_LEX_ITEM_SOURCE_PREFIX:
      return W_SEED_CST_SOURCE_PREFIX;
    case W_SEED_LEX_ITEM_TRIVIA:
      return W_SEED_CST_TRIVIA;
    case W_SEED_LEX_ITEM_WORD:
      return W_SEED_CST_WORD;
    case W_SEED_LEX_ITEM_NUMBER:
      return W_SEED_CST_NUMBER;
    case W_SEED_LEX_ITEM_PUNCTUATION:
      return W_SEED_CST_PUNCTUATION;
    case W_SEED_LEX_ITEM_LITERAL_EVENT:
      return W_SEED_CST_LITERAL_EVENT;
    case W_SEED_LEX_ITEM_FOREIGN_BODY:
      return W_SEED_CST_FOREIGN_BODY;
    case W_SEED_LEX_ITEM_UNKNOWN:
      return W_SEED_CST_UNKNOWN;
    case W_SEED_LEX_ITEM_EOF:
      return W_SEED_CST_ERROR;
  }
  return W_SEED_CST_ERROR;
}

static w_seed_span current_span(const w_seed_parser *parser) {
  if (parser->token_count == 0) {
    return empty_span(parser->lexer.bounds.end_byte);
  }
  return parser->token_cache[0].item.span;
}

static w_seed_span owner_span(const w_seed_parser *parser) {
  if (parser->frame_count == 0) {
    return empty_span(parser->lexer.bounds.start_byte);
  }
  return parser->nodes[parser->frames[parser->frame_count - 1].node].raw_span;
}

static void record_capacity(w_seed_parser *parser) {
  if (parser->status == W_SEED_PARSE_FATAL) return;
  parser->status = W_SEED_PARSE_FATAL;
  if (parser->issue_count < parser->issue_capacity) {
    w_seed_parse_issue *issue = &parser->issues[parser->issue_count];
    issue->kind = W_SEED_PARSE_ISSUE_CAPACITY;
    issue->primary = current_span(parser);
    issue->owner = owner_span(parser);
    issue->actual_kind = W_SEED_LEX_ITEM_UNKNOWN;
    issue->expected_mask = 0;
    parser->issue_count += 1;
  }
}

static bool record_issue(w_seed_parser *parser, w_seed_parse_issue_kind kind,
                         w_seed_span primary, uint32_t expected_mask) {
  if (parser->status == W_SEED_PARSE_FATAL) return false;
  if (parser->issue_count >= parser->issue_capacity) {
    record_capacity(parser);
    return false;
  }
  w_seed_parse_issue *issue = &parser->issues[parser->issue_count];
  issue->kind = kind;
  issue->primary = primary;
  issue->owner = owner_span(parser);
  issue->actual_kind = parser->token_count == 0
                           ? W_SEED_LEX_ITEM_EOF
                           : parser->token_cache[0].item.kind;
  issue->expected_mask = expected_mask;
  parser->issue_count += 1;
  if (parser->status == W_SEED_PARSE_COMPLETE) {
    parser->status = W_SEED_PARSE_RECOVERED;
  }
  return true;
}

static bool record_fatal(w_seed_parser *parser, w_seed_parse_issue_kind kind,
                         w_seed_span primary, uint32_t expected_mask) {
  (void)record_issue(parser, kind, primary, expected_mask);
  parser->status = W_SEED_PARSE_FATAL;
  return false;
}

static w_seed_cst_index add_node(w_seed_parser *parser, w_seed_cst_kind kind,
                                 uint16_t flags, w_seed_span span) {
  if (parser->node_count >= parser->node_capacity ||
      parser->node_count >= (size_t)UINT32_MAX) {
    record_capacity(parser);
    return W_SEED_CST_NONE;
  }
  const w_seed_cst_index index = (w_seed_cst_index)parser->node_count;
  w_seed_cst_node *node = &parser->nodes[parser->node_count];
  node->kind = kind;
  node->flags = flags;
  node->raw_span = span;
  node->first_child = W_SEED_CST_NONE;
  node->next_sibling = W_SEED_CST_NONE;
  parser->node_count += 1;

  if (parser->frame_count != 0) {
    w_seed_parse_frame *parent = &parser->frames[parser->frame_count - 1];
    w_seed_cst_node *parent_node = &parser->nodes[parent->node];
    if (parent->last_child == W_SEED_CST_NONE) {
      parent_node->first_child = index;
    } else {
      parser->nodes[parent->last_child].next_sibling = index;
    }
    parent->last_child = index;
  }
  return index;
}

static w_seed_cst_index push_node(w_seed_parser *parser, w_seed_cst_kind kind,
                                  size_t start) {
  const w_seed_cst_index node =
      add_node(parser, kind, 0, (w_seed_span){start, start});
  if (node == W_SEED_CST_NONE) return W_SEED_CST_NONE;
  if (parser->frame_count >= parser->frame_capacity) {
    record_capacity(parser);
    return W_SEED_CST_NONE;
  }
  w_seed_parse_frame *frame = &parser->frames[parser->frame_count];
  frame->kind = kind;
  frame->node = node;
  frame->last_child = W_SEED_CST_NONE;
  parser->frame_count += 1;
  return node;
}

static void pop_node(w_seed_parser *parser, size_t end) {
  if (parser->frame_count == 0) return;
  const w_seed_cst_index node = parser->frames[parser->frame_count - 1].node;
  size_t final_end = end;
  w_seed_cst_index child = parser->nodes[node].first_child;
  while (child != W_SEED_CST_NONE) {
    if (parser->nodes[child].raw_span.end_byte > final_end) {
      final_end = parser->nodes[child].raw_span.end_byte;
    }
    child = parser->nodes[child].next_sibling;
  }
  parser->nodes[node].raw_span.end_byte = final_end;
  parser->frame_count -= 1;
}

static w_seed_cst_index add_raw_span(w_seed_parser *parser, w_seed_cst_kind kind,
                                     uint16_t flags, w_seed_span span) {
  const w_seed_cst_index node = add_node(parser, kind,
                                         (uint16_t)(flags | W_SEED_CST_FLAG_RAW_LEAF),
                                         span);
  if (node != W_SEED_CST_NONE) parser->leaf_count += 1;
  return node;
}

static void append_missing(w_seed_parser *parser, size_t offset,
                           w_seed_parse_issue_kind kind) {
  if (parser->status == W_SEED_PARSE_FATAL) return;
  const w_seed_span span = empty_span(offset);
  (void)add_node(parser, W_SEED_CST_MISSING, W_SEED_CST_FLAG_MISSING, span);
  (void)record_issue(parser, kind, span, 0);
}

static bool fill_tokens(w_seed_parser *parser, size_t needed) {
  if (parser->status == W_SEED_PARSE_FATAL) return false;
  while (parser->token_count < needed) {
    if (parser->status == W_SEED_PARSE_FATAL) return false;
    if (parser->token_count >= parser->token_capacity) {
      record_capacity(parser);
      return false;
    }
    w_seed_lex_error error;
    if (!w_seed_lexer_next(&parser->lexer,
                           &parser->token_cache[parser->token_count].item,
                           &error)) {
      parser->status = W_SEED_PARSE_FATAL;
      if (parser->issue_count < parser->issue_capacity) {
        w_seed_parse_issue *issue = &parser->issues[parser->issue_count];
        issue->kind = W_SEED_PARSE_ISSUE_LEXER;
        issue->primary = error.primary;
        issue->owner = owner_span(parser);
        issue->actual_kind = W_SEED_LEX_ITEM_UNKNOWN;
        issue->expected_mask = 0;
        parser->issue_count += 1;
      }
      return false;
    }
    parser->token_count += 1;
  }
  return true;
}

static void shift_token(w_seed_parser *parser) {
  if (parser->token_count == 0) return;
  if (parser->token_count > 1) {
    (void)memmove(&parser->token_cache[0], &parser->token_cache[1],
                  (parser->token_count - 1) * sizeof(parser->token_cache[0]));
  }
  parser->token_count -= 1;
}

static bool skip_trivia(w_seed_parser *parser) {
  while (fill_tokens(parser, 1) && item_is_trivia(&parser->token_cache[0].item)) {
    const w_seed_lex_item item = parser->token_cache[0].item;
    uint16_t flags = 0;
    if (item.kind == W_SEED_LEX_ITEM_TRIVIA) flags = W_SEED_CST_FLAG_TRIVIA;
    (void)add_raw_span(parser, leaf_kind(item.kind), flags, item.span);
    shift_token(parser);
  }
  return parser->token_count != 0;
}

static bool current_is_eof(w_seed_parser *parser) {
  (void)skip_trivia(parser);
  return parser->token_count != 0 &&
         parser->token_cache[0].item.kind == W_SEED_LEX_ITEM_EOF;
}

static bool current_is_text(w_seed_parser *parser, const char *text) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  return span_text(parser, parser->token_cache[0].item.span, text);
}

static bool current_is_kind(w_seed_parser *parser, w_seed_lex_item_kind kind) {
  if (!skip_trivia(parser)) return false;
  return parser->token_count != 0 && parser->token_cache[0].item.kind == kind;
}

static bool next_significant(w_seed_parser *parser, w_seed_lex_item *item) {
  if (!skip_trivia(parser)) return false;
  size_t index = 1;
  while (true) {
    if (!fill_tokens(parser, index + 1)) return false;
    if (!item_is_trivia(&parser->token_cache[index].item)) {
      *item = parser->token_cache[index].item;
      return true;
    }
    index += 1;
  }
}

static bool next_is_text(w_seed_parser *parser, const char *text) {
  w_seed_lex_item item;
  if (!next_significant(parser, &item)) return false;
  return span_text(parser, item.span, text);
}

static bool next_adjacent_is_text(w_seed_parser *parser, const char *text) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  const w_seed_span current = parser->token_cache[0].item.span;
  const size_t length = strlen(text);
  if (current.end_byte > parser->lexer.bounds.end_byte ||
      length > parser->lexer.bounds.end_byte - current.end_byte) {
    return false;
  }
  const w_seed_span adjacent = {current.end_byte, current.end_byte + length};
  return span_text(parser, adjacent, text);
}

static bool next_two_are_text(w_seed_parser *parser, const char *first,
                              const char *second) {
  size_t index = 1;
  size_t significant = 0;
  while (true) {
    if (!fill_tokens(parser, index + 1)) return false;
    const w_seed_lex_item *item = &parser->token_cache[index].item;
    if (item->kind == W_SEED_LEX_ITEM_EOF) return false;
    if (item_is_trivia(item)) {
      index += 1;
      continue;
    }
    if (significant == 0) {
      if (!span_text(parser, item->span, first)) return false;
    } else {
      return span_text(parser, item->span, second);
    }
    significant += 1;
    index += 1;
  }
}

static bool consume_raw(w_seed_parser *parser, uint16_t flags,
                        w_seed_cst_kind override_kind, w_seed_span *span_out) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  const w_seed_lex_item item = parser->token_cache[0].item;
  const w_seed_cst_kind kind =
      (flags & W_SEED_CST_FLAG_ERROR) != 0
          ? W_SEED_CST_ERROR
          : (override_kind == W_SEED_CST_ERROR ? leaf_kind(item.kind)
                                               : override_kind);
  const w_seed_cst_index leaf = add_raw_span(parser, kind, flags, item.span);
  if (leaf == W_SEED_CST_NONE) return false;
  parser->last_token_end = item.span.end_byte;
  parser->has_last_token = true;
  shift_token(parser);
  if (span_out != NULL) *span_out = item.span;
  return true;
}

static bool consume_current(w_seed_parser *parser, w_seed_span *span_out) {
  return consume_raw(parser, 0, W_SEED_CST_ERROR, span_out);
}

static bool current_is_double_gt(w_seed_parser *parser) {
  if (!skip_trivia(parser) || current_is_eof(parser) ||
      parser->token_cache[0].item.kind != W_SEED_LEX_ITEM_PUNCTUATION) {
    return false;
  }
  const w_seed_span span = parser->token_cache[0].item.span;
  return span.end_byte == span.start_byte + 2 &&
         span_text(parser, span, ">>");
}

static bool consume_virtual_close(w_seed_parser *parser,
                                  w_seed_parse_token_view *view) {
  if (!skip_trivia(parser)) return false;
  if (!parser->virtual_close_active) {
    if (!current_is_double_gt(parser)) return false;
    const w_seed_lex_item item = parser->token_cache[0].item;
    parser->virtual_close_leaf = add_raw_span(
        parser, W_SEED_CST_PUNCTUATION, 0, item.span);
    if (parser->virtual_close_leaf == W_SEED_CST_NONE) return false;
    parser->virtual_close_active = true;
    parser->virtual_close_offset = 0;
  }
  const w_seed_span raw = parser->token_cache[0].item.span;
  const size_t start = raw.start_byte + parser->virtual_close_offset;
  const w_seed_span part = {start, start + 1};
  parser->last_token_end = part.end_byte;
  parser->has_last_token = true;
  if (view != NULL) {
    view->span = part;
    view->origin_leaf = parser->virtual_close_leaf;
    view->flags = W_SEED_PARSE_TOKEN_VIEW_VIRTUAL;
  }
  parser->virtual_close_offset += 1;
  if (parser->virtual_close_offset == 2) {
    parser->virtual_close_active = false;
    parser->virtual_close_offset = 0;
    shift_token(parser);
  }
  return true;
}

static bool consume_text(w_seed_parser *parser, const char *text,
                         w_seed_span *span_out) {
  if (!current_is_text(parser, text)) return false;
  return consume_current(parser, span_out);
}

static bool expect_text(w_seed_parser *parser, const char *text,
                        w_seed_parse_issue_kind missing_kind) {
  if (parser->status == W_SEED_PARSE_FATAL) return false;
  if (consume_text(parser, text, NULL)) return true;
  append_missing(parser, current_span(parser).start_byte, missing_kind);
  return false;
}

static bool is_unsupported_word(w_seed_parser *parser) {
  static const char *const words[] = {
      "object",    "service",  "enum",
      "protocol", "type",      "alias",     "dimension", "unit",
      "extension", "behavior", "const",
      "static",   "unsafe",    "async",     "mut",      "take",
      "allocator", "spawn",    "switch",    "for",      "transaction",
      "foreign",  "captures",  "package",  "workspace",
  };
  for (size_t index = 0; index < sizeof(words) / sizeof(words[0]); index += 1) {
    if (current_is_text(parser, words[index])) return true;
  }
  return false;
}

static void stop_with_remainder(w_seed_parser *parser,
                                w_seed_parse_issue_kind kind) {
  (void)skip_trivia(parser);
  const w_seed_span current = current_span(parser);
  const size_t start = parser->virtual_close_active ? current.end_byte
                                                     : current.start_byte;
  const size_t end = parser->lexer.bounds.end_byte;
  (void)record_fatal(parser, kind, (w_seed_span){start, end}, 0);
  if (start < end) {
    (void)add_raw_span(parser, W_SEED_CST_ERROR,
                       W_SEED_CST_FLAG_ERROR, (w_seed_span){start, end});
  }
  parser->token_count = 0;
  parser->lexer.cursor = end;
  parser->lexer.terminal = true;
}

static void preserve_remainder(w_seed_parser *parser) {
  (void)skip_trivia(parser);
  const w_seed_span current = current_span(parser);
  const size_t start = parser->virtual_close_active ? current.end_byte
                                                     : current.start_byte;
  const size_t end = parser->lexer.bounds.end_byte;
  if (start < end) {
    (void)add_raw_span(parser, W_SEED_CST_ERROR, W_SEED_CST_FLAG_ERROR,
                       (w_seed_span){start, end});
  }
  parser->token_count = 0;
  parser->lexer.cursor = end;
  parser->lexer.terminal = true;
}

static bool parse_expression(w_seed_parser *parser, int minimum_precedence,
                             bool value_context);
static bool parse_type(w_seed_parser *parser);
static bool parse_block(w_seed_parser *parser, bool value_context);
static bool parse_lock_expression(w_seed_parser *parser);
static bool parse_generic_parameters(w_seed_parser *parser,
                                     size_t declaration_end);
static bool parse_contract_envelope(w_seed_parser *parser, size_t head_end,
                                    bool expression_mode);
static bool parse_static_value(w_seed_parser *parser);
static bool parse_static_list(w_seed_parser *parser);
static bool parse_switch_expression(w_seed_parser *parser);
static bool parse_allocator_block(w_seed_parser *parser);
static bool parse_binding_statement(w_seed_parser *parser, const char *keyword,
                                    w_seed_cst_kind kind);
static bool parse_let_statement(w_seed_parser *parser);
static bool parse_var_statement(w_seed_parser *parser);
static bool parse_spawn_statement(w_seed_parser *parser);
static bool statement_boundary(w_seed_parser *parser);
static bool parse_borrow_clause(w_seed_parser *parser);
static bool parse_function_type(w_seed_parser *parser, size_t start);
static bool parse_closure_expression(w_seed_parser *parser, bool value_context);
static bool parse_capture_expression(w_seed_parser *parser, bool value_context);
static bool parse_enum_declaration(w_seed_parser *parser);

static bool parse_transaction_expression(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_TRANSACTION_EXPRESSION, start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "transaction", NULL);
  if (current_is_text(parser, "<")) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    return false;
  }
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (!expect_text(parser, "=", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
      !parse_expression(parser, 1, false) ||
      !parse_block(parser, false)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_allocator_block(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_ALLOCATOR_BLOCK, start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "allocator", NULL);

  if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD) &&
      next_is_text(parser, ":")) {
    (void)consume_current(parser, NULL);
    if (!expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  } else if (next_is_text(parser, ":")) {
    (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                       current_span(parser), W_SEED_PARSE_EXPECT_WORD);
    (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR, NULL);
    (void)consume_text(parser, ":", NULL);
  }

  if (current_is_text(parser, "{")) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
  } else if (!parse_expression(parser, 1, false)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }

  if (current_is_text(parser, ":")) {
    (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                       current_span(parser), W_SEED_PARSE_EXPECT_PUNCTUATION);
    (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR, NULL);
  }
  if (!parse_block(parser, false)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)statement_boundary(parser);
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_lock_expression(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_LOCK_EXPRESSION, start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "lock", NULL);
  if (current_is_text(parser, "as")) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
  } else if (!parse_expression(parser, 1, false)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (!expect_text(parser, "as", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (!parse_block(parser, true)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static binary_info binary_operator(w_seed_parser *parser) {
  binary_info info = {false, -1};
  if (!skip_trivia(parser) || current_is_eof(parser)) return info;
  static const struct {
    const char *text;
    int precedence;
    bool right_associative;
  } operators[] = {
      {"=", 1, true},    {"+=", 1, true},  {"-=", 1, true},
      {"*=", 1, true},   {"/=", 1, true},  {"%=", 1, true},
      {"**=", 1, true},  {"<<=", 1, true}, {">>=", 1, true},
      {"&=", 1, true},   {"^=", 1, true},  {"|=", 1, true},
      {"??", 2, true},   {"||", 3, false}, {"&&", 4, false},
      {"|", 5, false},   {"^", 6, false},  {"&", 7, false},
      {"==", 8, false},  {"!=", 8, false}, {"<", 9, false},
      {"<=", 9, false},  {">", 9, false},  {">=", 9, false},
      {"...", 10, false}, {"..<", 10, false}, {">..", 10, false},
      {">..<", 10, false}, {"<<", 11, false}, {">>", 11, false},
      {"+", 12, false},  {"-", 12, false}, {"*", 13, false},
      {"/", 13, false},  {"%", 13, false}, {"@", 13, false},
      {"**", 14, true},
  };
  for (size_t index = 0; index < sizeof(operators) / sizeof(operators[0]);
       index += 1) {
    if (current_is_text(parser, operators[index].text)) {
      info.precedence = operators[index].precedence;
      info.right_associative = operators[index].right_associative;
      return info;
    }
  }
  if (current_is_text(parser, "in") || current_is_text(parser, "is")) {
    info.precedence = 9;
  }
  return info;
}

static bool parse_switch_expression(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_SWITCH_EXPRESSION, start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "switch", NULL);
  if (!parse_expression(parser, 1, false) ||
      !expect_text(parser, "{", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  size_t arm_count = 0;
  while (!current_is_eof(parser) && !current_is_text(parser, "}")) {
    const size_t arm_start = current_span(parser).start_byte;
    if (!current_is_text(parser, "case")) {
      append_missing(parser, current_span(parser).start_byte,
                     W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
    if (push_node(parser, W_SEED_CST_SWITCH_ARM, arm_start) ==
        W_SEED_CST_NONE)
      return false;
    (void)consume_text(parser, "case", NULL);
    if (current_is_text(parser, ".")) {
      (void)consume_text(parser, ".", NULL);
      if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
        append_missing(parser, current_span(parser).start_byte,
                       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
        pop_node(parser, parser->has_last_token ? parser->last_token_end : arm_start);
        return false;
      }
      (void)consume_current(parser, NULL);
    } else if (current_is_kind(parser, W_SEED_LEX_ITEM_NUMBER) ||
               current_is_text(parser, "true") ||
               current_is_text(parser, "false")) {
      (void)consume_current(parser, NULL);
    } else if (current_is_kind(parser, W_SEED_LEX_ITEM_LITERAL_EVENT)) {
      do {
        (void)consume_current(parser, NULL);
      } while (current_is_kind(parser, W_SEED_LEX_ITEM_LITERAL_EVENT));
    } else {
      append_missing(parser, current_span(parser).start_byte,
                     W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
      pop_node(parser, parser->has_last_token ? parser->last_token_end : arm_start);
      return false;
    }
    if (!expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : arm_start);
      return false;
    }
    if (current_is_text(parser, "case") || current_is_text(parser, "}")) {
      append_missing(parser, current_span(parser).start_byte,
                     W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
      pop_node(parser, parser->has_last_token ? parser->last_token_end : arm_start);
      return false;
    }
    if (!parse_expression(parser, 1, false)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : arm_start);
      return false;
    }
    if (current_is_text(parser, ";")) (void)consume_text(parser, ";", NULL);
    pop_node(parser, parser->last_token_end);
    arm_count += 1;
  }
  if (arm_count == 0) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (!expect_text(parser, "}", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_closure_parameter(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_CLOSURE_PARAMETER, start) ==
      W_SEED_CST_NONE)
    return false;
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (current_is_text(parser, ":")) {
    (void)consume_text(parser, ":", NULL);
    if (!parse_type(parser)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_closure_parameters(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_CLOSURE_PARAMETERS, start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "(", NULL);
  if (!current_is_text(parser, ")")) {
    while (true) {
      if (!parse_closure_parameter(parser)) {
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
      if (current_is_text(parser, ",")) {
        (void)consume_text(parser, ",", NULL);
        if (current_is_text(parser, ")")) break;
        continue;
      }
      if (current_is_text(parser, ")")) break;
      append_missing(parser, current_span(parser).start_byte,
                     W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  if (!expect_text(parser, ")", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_closure_expression(w_seed_parser *parser, bool value_context) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_CLOSURE_EXPRESSION, start) ==
      W_SEED_CST_NONE)
    return false;
  if (!parse_closure_parameters(parser) ||
      !expect_text(parser, "=>", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (current_is_text(parser, "{")) {
    if (!parse_block(parser, true)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  } else if (!parse_expression(parser, 1, true)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  (void)value_context;
  return true;
}

static bool parse_capture_item(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_CAPTURE_ITEM, start) == W_SEED_CST_NONE)
    return false;
  if (!(current_is_text(parser, "copy") || current_is_text(parser, "ref") ||
        current_is_text(parser, "take") || current_is_text(parser, "weak"))) {
    (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                       current_span(parser), W_SEED_PARSE_EXPECT_WORD);
    if (!current_is_eof(parser) && !current_is_text(parser, "]")) {
      (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR, NULL);
    }
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)consume_current(parser, NULL);
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_capture_expression(w_seed_parser *parser, bool value_context) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_CAPTURE_EXPRESSION, start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "<", NULL);
  (void)consume_text(parser, "[", NULL);
  if (current_is_text(parser, "]")) {
    (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                       current_span(parser), W_SEED_PARSE_EXPECT_WORD);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  while (true) {
    if (!parse_capture_item(parser)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
    if (!current_is_text(parser, ",")) break;
    (void)consume_text(parser, ",", NULL);
    if (current_is_text(parser, "]")) break;
  }
  if (!expect_text(parser, "]", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE) ||
      !expect_text(parser, ">", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE) ||
      !parse_closure_expression(parser, value_context)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_primary(w_seed_parser *parser, bool value_context) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  if (current_is_text(parser, "spawn")) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    return false;
  }
  if (current_is_text(parser, "switch")) {
    return parse_switch_expression(parser);
  }
  if (current_is_text(parser, "if")) {
    const size_t start = current_span(parser).start_byte;
    const w_seed_cst_index node = push_node(parser, W_SEED_CST_IF_EXPRESSION, start);
    if (node == W_SEED_CST_NONE) return false;
    (void)consume_text(parser, "if", NULL);
    if (!parse_expression(parser, 1, false)) return false;
    if (!parse_block(parser, true)) return false;
    if (!current_is_text(parser, "else")) {
      const size_t offset = current_span(parser).start_byte;
      (void)record_issue(parser, W_SEED_PARSE_ISSUE_VALUE_IF_MISSING_ELSE,
                         empty_span(offset), W_SEED_PARSE_EXPECT_PUNCTUATION);
      (void)add_node(parser, W_SEED_CST_MISSING, W_SEED_CST_FLAG_MISSING,
                     empty_span(offset));
    } else {
      (void)consume_text(parser, "else", NULL);
      if (current_is_text(parser, "if")) {
        if (!parse_primary(parser, true)) return false;
      } else if (!parse_block(parser, true)) {
        return false;
      }
    }
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return true;
  }
  if (current_is_text(parser, "transaction")) {
    return parse_transaction_expression(parser);
  }
  if (current_is_text(parser, "lock")) {
    return parse_lock_expression(parser);
  }
  if (current_is_text(parser, "<") && next_adjacent_is_text(parser, "[")) {
    return parse_capture_expression(parser, value_context);
  }
  if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD) ||
      current_is_kind(parser, W_SEED_LEX_ITEM_NUMBER) ||
      current_is_kind(parser, W_SEED_LEX_ITEM_LITERAL_EVENT)) {
    if (current_is_kind(parser, W_SEED_LEX_ITEM_LITERAL_EVENT)) {
      (void)consume_current(parser, NULL);
      while (current_is_kind(parser, W_SEED_LEX_ITEM_LITERAL_EVENT)) {
        (void)consume_current(parser, NULL);
      }
    } else {
      (void)consume_current(parser, NULL);
    }
    return true;
  }
  if (current_is_text(parser, ".")) {
    (void)consume_current(parser, NULL);
    if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
      (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                         current_span(parser), W_SEED_PARSE_EXPECT_WORD);
      return false;
    }
    (void)consume_current(parser, NULL);
    return true;
  }
  if (current_is_text(parser, "(")) {
    const size_t start = current_span(parser).start_byte;
    const w_seed_cst_index node = push_node(parser, W_SEED_CST_PARENTHESES, start);
    if (node == W_SEED_CST_NONE) return false;
    (void)consume_text(parser, "(", NULL);
    if (!current_is_text(parser, ")") &&
        !parse_expression(parser, 1, false)) {
      pop_node(parser, start);
      return false;
    }
    if (current_is_text(parser, ",")) {
      parser->nodes[node].kind = W_SEED_CST_TUPLE_EXPRESSION;
      (void)consume_text(parser, ",", NULL);
      if (current_is_text(parser, ")")) {
        (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                           current_span(parser), W_SEED_PARSE_EXPECT_EXPRESSION);
        (void)consume_text(parser, ")", NULL);
        pop_node(parser, parser->last_token_end);
        return false;
      }
      if (!parse_expression(parser, 1, false)) {
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
      while (current_is_text(parser, ",")) {
        (void)consume_text(parser, ",", NULL);
        if (current_is_text(parser, ")")) break;
        if (!parse_expression(parser, 1, false)) {
          pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
          return false;
        }
      }
    }
    if (!expect_text(parser, ")", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
    pop_node(parser, parser->last_token_end);
    return true;
  }
  if (current_is_text(parser, "[")) {
    const size_t start = current_span(parser).start_byte;
    const w_seed_cst_index node = push_node(parser, W_SEED_CST_ARRAY, start);
    if (node == W_SEED_CST_NONE) return false;
    (void)consume_text(parser, "[", NULL);
    if (current_is_text(parser, ";")) {
      stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
      return false;
    }
    if (!current_is_text(parser, "]")) {
      if (!parse_expression(parser, 1, false)) return false;
      if (current_is_text(parser, ";")) {
        (void)consume_text(parser, ";", NULL);
        if (!parse_expression(parser, 1, false)) return false;
      } else {
        while (current_is_text(parser, ",")) {
          (void)consume_text(parser, ",", NULL);
          if (current_is_text(parser, "]")) break;
          if (!parse_expression(parser, 1, false)) return false;
        }
      }
    }
    if (!expect_text(parser, "]", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
    pop_node(parser, parser->last_token_end);
    return true;
  }
  (void)value_context;
  (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                     current_span(parser), W_SEED_PARSE_EXPECT_EXPRESSION);
  if (!current_is_eof(parser)) (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR,
                                                  W_SEED_CST_ERROR, NULL);
  return false;
}

static bool parse_argument(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_ARGUMENT, start) == W_SEED_CST_NONE)
    return false;
  if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD) &&
      next_is_text(parser, ":")) {
    (void)consume_current(parser, NULL);
    if (!expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN)) {
      pop_node(parser, parser->last_token_end);
      return false;
    }
  }
  if (!parse_expression(parser, 1, false)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_postfix(w_seed_parser *parser, bool value_context) {
  if (!parse_primary(parser, value_context)) return false;
  while (true) {
    if (current_is_text(parser, "<") &&
        current_span(parser).start_byte == parser->last_token_end) {
      if (!parse_contract_envelope(parser, parser->last_token_end, true)) {
        return false;
      }
      continue;
    }
    if (current_is_text(parser, "(")) {
      (void)consume_text(parser, "(", NULL);
      if (!current_is_text(parser, ")")) {
        if (!parse_argument(parser)) return false;
        while (current_is_text(parser, ",")) {
          (void)consume_text(parser, ",", NULL);
          if (current_is_text(parser, ")")) break;
          if (!parse_argument(parser)) return false;
        }
      }
      if (!expect_text(parser, ")", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
        return false;
      }
      continue;
    }
    if (current_is_text(parser, "[")) {
      (void)consume_text(parser, "[", NULL);
      if (!current_is_text(parser, "]")) {
        if (!parse_expression(parser, 1, false)) return false;
        while (current_is_text(parser, ",")) {
          (void)consume_text(parser, ",", NULL);
          if (current_is_text(parser, "]")) break;
          if (!parse_expression(parser, 1, false)) return false;
        }
      }
      if (!expect_text(parser, "]", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
        return false;
      }
      continue;
    }
    if (current_is_text(parser, ".") || current_is_text(parser, "?.")) {
      (void)consume_current(parser, NULL);
      if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
        (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                           current_span(parser), W_SEED_PARSE_EXPECT_WORD);
        return false;
      }
      (void)consume_current(parser, NULL);
      continue;
    }
    if (current_is_text(parser, "?")) {
      if (!parser->has_last_token || current_span(parser).start_byte !=
                                         parser->last_token_end) {
        (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                           current_span(parser), W_SEED_PARSE_EXPECT_EXPRESSION);
        (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR, NULL);
        continue;
      }
      (void)consume_text(parser, "?", NULL);
      continue;
    }
    break;
  }
  return true;
}

static bool parse_prefix(w_seed_parser *parser, bool value_context) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  if (current_is_text(parser, "try") && next_is_text(parser, "allocator")) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    return false;
  }
  if (current_is_text(parser, "!") || current_is_text(parser, "~") ||
      current_is_text(parser, "-") || current_is_text(parser, "copy") ||
      current_is_text(parser, "take") || current_is_text(parser, "pin") ||
      current_is_text(parser, "inout") || current_is_text(parser, "ref")) {
    (void)consume_current(parser, NULL);
    return parse_prefix(parser, value_context);
  }
  if (current_is_text(parser, "try") || current_is_text(parser, "await")) {
    (void)consume_current(parser, NULL);
    if (current_is_text(parser, "?") && parser->has_last_token &&
        current_span(parser).start_byte == parser->last_token_end) {
      (void)consume_current(parser, NULL);
    }
    return parse_prefix(parser, value_context);
  }
  return parse_postfix(parser, value_context);
}

static bool parse_expression(w_seed_parser *parser, int minimum_precedence,
                             bool value_context) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  const size_t start = current_span(parser).start_byte;
  const w_seed_cst_index node = push_node(parser, W_SEED_CST_EXPRESSION, start);
  if (node == W_SEED_CST_NONE) return false;
  if (!parse_prefix(parser, value_context)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  while (true) {
    const binary_info info = binary_operator(parser);
    if (info.precedence < minimum_precedence) break;
    (void)consume_current(parser, NULL);
    const int next_precedence =
        info.right_associative ? info.precedence : info.precedence + 1;
    if (!parse_expression(parser, next_precedence, false)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_static_value(w_seed_parser *parser) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  if (current_is_text(parser, "[")) return parse_static_list(parser);
  if (current_is_text(parser, "(")) {
    (void)consume_text(parser, "(", NULL);
    if (!parse_expression(parser, 1, false) ||
        !expect_text(parser, ")", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
      return false;
    }
    return true;
  }
  if (current_is_text(parser, ".")) {
    (void)consume_text(parser, ".", NULL);
    if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
      (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                         current_span(parser), W_SEED_PARSE_EXPECT_WORD);
      return false;
    }
    (void)consume_current(parser, NULL);
    return true;
  }
  if (current_is_kind(parser, W_SEED_LEX_ITEM_LITERAL_EVENT)) {
    do {
      (void)consume_current(parser, NULL);
    } while (current_is_kind(parser, W_SEED_LEX_ITEM_LITERAL_EVENT));
    return true;
  }
  if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD) ||
      current_is_kind(parser, W_SEED_LEX_ITEM_NUMBER)) {
    (void)consume_current(parser, NULL);
    while (current_is_text(parser, ".")) {
      (void)consume_text(parser, ".", NULL);
      if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
        (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                           current_span(parser), W_SEED_PARSE_EXPECT_WORD);
        return false;
      }
      (void)consume_current(parser, NULL);
    }
    /* Quantity-like values retain their attached contract envelope. */
    while (current_is_text(parser, "<") &&
           current_span(parser).start_byte == parser->last_token_end) {
      if (!parse_contract_envelope(parser, parser->last_token_end, false)) {
        return false;
      }
    }
    return true;
  }
  (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                     current_span(parser), W_SEED_PARSE_EXPECT_EXPRESSION);
  return false;
}

static bool parse_static_list(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_ARRAY, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "[", NULL);
  if (!current_is_text(parser, "]")) {
    if (!parse_static_value(parser)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
    while (current_is_text(parser, ",")) {
      (void)consume_text(parser, ",", NULL);
      if (current_is_text(parser, "]")) break;
      if (!parse_static_value(parser)) {
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
    }
  }
  if (!expect_text(parser, "]", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_contract_argument(w_seed_parser *parser,
                                    bool expression_mode) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD) &&
      next_is_text(parser, ":")) {
    (void)consume_current(parser, NULL);
    if (!expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN))
      return false;
    return parse_static_value(parser);
  }
  if (!expression_mode && current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    return parse_type(parser);
  }
  return parse_static_value(parser);
}

static bool parse_contract_envelope(w_seed_parser *parser, size_t head_end,
                                    bool expression_mode) {
  if (!current_is_text(parser, "<")) return false;
  const size_t start = current_span(parser).start_byte;
  if (start != head_end) {
    (void)record_issue(parser, W_SEED_PARSE_ISSUE_SPACED_HEAD,
                       current_span(parser), W_SEED_PARSE_EXPECT_PUNCTUATION);
    return false;
  }
  if (push_node(parser, W_SEED_CST_CONTRACT_ENVELOPE, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "<", NULL);
  if (current_is_text(parser, ">") || current_is_double_gt(parser)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  if (!parse_contract_argument(parser, expression_mode)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  while (current_is_text(parser, ",")) {
    (void)consume_text(parser, ",", NULL);
    if (current_is_text(parser, ">") || current_is_double_gt(parser)) break;
    if (!parse_contract_argument(parser, expression_mode)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  if (current_is_text(parser, ">")) {
    (void)consume_text(parser, ">", NULL);
  } else if (current_is_double_gt(parser)) {
    w_seed_parse_token_view view;
    (void)consume_virtual_close(parser, &view);
  } else {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_function_type_parameters(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_FUNCTION_TYPE_PARAMETERS, start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "(", NULL);
  if (!current_is_text(parser, ")")) {
    while (true) {
      /* Function-type parameter labels use the same `named value: Type`
       * surface as declaration parameters.  Preserve the label in the raw
       * owner span while keeping the direct TYPE child authoritative. */
      w_seed_lex_item next;
      const bool named_parameter =
          current_is_text(parser, "named") && next_significant(parser, &next) &&
          next.kind == W_SEED_LEX_ITEM_WORD;
      if (named_parameter) {
        (void)consume_current(parser, NULL);
        if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD) ||
            !next_is_text(parser, ":")) {
          append_missing(parser, current_span(parser).start_byte,
                         W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
          pop_node(parser, parser->has_last_token ? parser->last_token_end
                                                   : start);
          return false;
        }
        (void)consume_current(parser, NULL);
        if (!expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN)) {
          pop_node(parser, parser->has_last_token ? parser->last_token_end
                                                   : start);
          return false;
        }
      } else if (current_is_text(parser, "ref") || current_is_text(parser, "inout") ||
          current_is_text(parser, "take") || current_is_text(parser, "const")) {
        (void)consume_current(parser, NULL);
      }
      if (!parse_type(parser)) {
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
      if (current_is_text(parser, ",")) {
        (void)consume_text(parser, ",", NULL);
        if (current_is_text(parser, ")")) break;
        continue;
      }
      if (current_is_text(parser, ")")) break;
      append_missing(parser, current_span(parser).start_byte,
                     W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  if (!expect_text(parser, ")", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_function_type(w_seed_parser *parser, size_t start) {
  if (push_node(parser, W_SEED_CST_FUNCTION_TYPE, start) == W_SEED_CST_NONE)
    return false;
  if (current_is_text(parser, "some") || current_is_text(parser, "any")) {
    (void)consume_current(parser, NULL);
  }
  if (current_is_text(parser, "mut") || current_is_text(parser, "take")) {
    (void)consume_current(parser, NULL);
  }
  if (!expect_text(parser, "fn", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
      !current_is_text(parser, "(")) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (!parse_function_type_parameters(parser)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (current_is_text(parser, ":")) {
    (void)consume_text(parser, ":", NULL);
    if (!parse_type(parser)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  if (current_is_text(parser, "throws")) {
    (void)consume_text(parser, "throws", NULL);
    if (!parse_type(parser)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  if (current_is_text(parser, "borrows") && !parse_borrow_clause(parser)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_type(w_seed_parser *parser) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  const size_t start = current_span(parser).start_byte;
  const w_seed_cst_index node = push_node(parser, W_SEED_CST_TYPE, start);
  if (node == W_SEED_CST_NONE) return false;
  bool function_type = current_is_text(parser, "fn") ||
                       current_is_text(parser, "mut") ||
                       current_is_text(parser, "take");
  if (current_is_text(parser, "some") || current_is_text(parser, "any")) {
    w_seed_lex_item next;
    if (next_significant(parser, &next) &&
        (span_text(parser, next.span, "fn") ||
         span_text(parser, next.span, "mut") ||
         span_text(parser, next.span, "take"))) {
      function_type = true;
    }
  }
  if (function_type) {
    if (!parse_function_type(parser, start)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return true;
  }
  if (current_is_text(parser, "view") || current_is_text(parser, "shared"))
    (void)consume_current(parser, NULL);
  if (current_is_text(parser, "(")) {
    const size_t tuple_start = current_span(parser).start_byte;
    if (next_is_text(parser, ")")) {
      (void)consume_text(parser, "(", NULL);
      if (!expect_text(parser, ")", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
    } else {
      if (push_node(parser, W_SEED_CST_TUPLE_TYPE, tuple_start) ==
          W_SEED_CST_NONE)
        return false;
      (void)consume_text(parser, "(", NULL);
      if (!parse_type(parser) ||
          !expect_text(parser, ",", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN)) {
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
      if (current_is_text(parser, ")")) {
        (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                           current_span(parser), W_SEED_PARSE_EXPECT_WORD);
        (void)consume_text(parser, ")", NULL);
        pop_node(parser, parser->last_token_end);
        pop_node(parser, parser->last_token_end);
        return false;
      }
      if (!parse_type(parser)) {
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
      while (current_is_text(parser, ",")) {
        (void)consume_text(parser, ",", NULL);
        if (current_is_text(parser, ")")) break;
        if (!parse_type(parser)) {
          pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
          pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
          return false;
        }
      }
      if (!expect_text(parser, ")", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
      pop_node(parser, parser->last_token_end);
    }
  } else {
    if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
      (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                         current_span(parser), W_SEED_PARSE_EXPECT_WORD);
      pop_node(parser, start);
      return false;
    }
    (void)consume_current(parser, NULL);
    while (current_is_text(parser, ".")) {
      (void)consume_text(parser, ".", NULL);
      if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
        (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                           current_span(parser), W_SEED_PARSE_EXPECT_WORD);
        pop_node(parser, parser->last_token_end);
        return false;
      }
      (void)consume_current(parser, NULL);
    }
    while (current_is_text(parser, "<")) {
      if (!parse_contract_envelope(parser, parser->last_token_end, false)) {
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
    }
  }
  if (current_is_text(parser, "?")) {
    if (current_span(parser).start_byte == parser->last_token_end) {
      (void)consume_text(parser, "?", NULL);
    }
  }
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_slot_ref(w_seed_parser *parser) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_SLOT_REF, start) == W_SEED_CST_NONE)
    return false;
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD) &&
      !current_is_kind(parser, W_SEED_LEX_ITEM_NUMBER)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)consume_current(parser, NULL);
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_borrow_pair(w_seed_parser *parser) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_BORROW_PAIR, start) == W_SEED_CST_NONE)
    return false;
  if (!parse_slot_ref(parser) ||
      !expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
      !expect_text(parser, "[", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
      !parse_slot_ref(parser)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  while (current_is_text(parser, ",")) {
    (void)consume_text(parser, ",", NULL);
    if (current_is_text(parser, "]")) break;
    if (!parse_slot_ref(parser)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  if (!expect_text(parser, "]", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_borrow_clause(w_seed_parser *parser) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_BORROW_CLAUSE, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "borrows", NULL);
  if (!expect_text(parser, "(", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
      !parse_borrow_pair(parser)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  while (current_is_text(parser, ",")) {
    (void)consume_text(parser, ",", NULL);
    if (current_is_text(parser, ")")) break;
    if (!parse_borrow_pair(parser)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  if (!expect_text(parser, ")", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool statement_boundary(w_seed_parser *parser) {
  if (current_is_text(parser, ";")) {
    (void)consume_text(parser, ";", NULL);
    return true;
  }
  if (current_is_text(parser, "else") || current_is_text(parser, "catch") ||
      current_is_text(parser, "while")) {
    (void)record_issue(parser, W_SEED_PARSE_ISSUE_NO_CONTINUATION_OWNER,
                       current_span(parser), W_SEED_PARSE_EXPECT_STATEMENT);
    (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR, NULL);
    return false;
  }
  return true;
}

static bool parse_binding_statement(w_seed_parser *parser, const char *keyword,
                                    w_seed_cst_kind kind) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, kind, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, keyword, NULL);
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (current_is_text(parser, ":")) {
    (void)consume_text(parser, ":", NULL);
    if (!parse_type(parser)) return false;
  }
  if (!expect_text(parser, "=", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
      !parse_expression(parser, 1, true)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)statement_boundary(parser);
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_let_statement(w_seed_parser *parser) {
  return parse_binding_statement(parser, "let", W_SEED_CST_LET_STATEMENT);
}

static bool parse_var_statement(w_seed_parser *parser) {
  return parse_binding_statement(parser, "var", W_SEED_CST_VAR_STATEMENT);
}

static bool parse_spawn_statement(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_SPAWN_STATEMENT, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "spawn", NULL);
  if (!current_is_text(parser, "<")) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    pop_node(parser, parser->lexer.bounds.end_byte);
    return false;
  }
  if (!parse_contract_envelope(parser, parser->last_token_end, true)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (!current_is_text(parser, "let")) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (!parse_let_statement(parser)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_return_statement(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_RETURN_STATEMENT, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "return", NULL);
  if (!current_is_text(parser, ";") && !current_is_text(parser, "}") &&
      !current_is_eof(parser)) {
    if (!parse_expression(parser, 1, true)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  (void)statement_boundary(parser);
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_commit_statement(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_COMMIT_STATEMENT, start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "commit", NULL);
  if (!current_is_text(parser, ";") && !current_is_text(parser, "}") &&
      !current_is_eof(parser)) {
    if (!parse_expression(parser, 1, true)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  (void)statement_boundary(parser);
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_if_statement(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_IF_STATEMENT, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "if", NULL);
  if (!parse_expression(parser, 1, false) || !parse_block(parser, false)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (current_is_text(parser, "else")) {
    (void)consume_text(parser, "else", NULL);
    if (current_is_text(parser, "if")) {
      if (!parse_if_statement(parser)) return false;
    } else if (!parse_block(parser, false)) {
      return false;
    }
  }
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_repeat_after_keyword(w_seed_parser *parser, size_t start) {
  if (push_node(parser, W_SEED_CST_REPEAT_STATEMENT, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "repeat", NULL);
  if (!parse_block(parser, false)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (!expect_text(parser, "while", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE) ||
      !parse_expression(parser, 1, false)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)statement_boundary(parser);
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_repeat_statement(w_seed_parser *parser) {
  return parse_repeat_after_keyword(parser, current_span(parser).start_byte);
}

static bool parse_for_statement(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_FOR_STATEMENT, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "for", NULL);
  if (current_is_text(parser, "async") || current_is_text(parser, "await") ||
      current_is_text(parser, "try") || current_is_text(parser, "take")) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    return false;
  }
  if (current_is_text(parser, "ref") || current_is_text(parser, "inout") ||
      current_is_text(parser, "copy")) {
    (void)consume_current(parser, NULL);
  }
  if (current_is_text(parser, "in") ||
      !current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
  } else {
    (void)consume_current(parser, NULL);
  }
  if (!expect_text(parser, "in", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
      !parse_expression(parser, 1, false) || !parse_block(parser, false)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_break_statement(w_seed_parser *parser, bool continuation) {
  const size_t start = current_span(parser).start_byte;
  const w_seed_cst_kind kind = continuation ? W_SEED_CST_CONTINUE_STATEMENT
                                             : W_SEED_CST_BREAK_STATEMENT;
  if (push_node(parser, kind, start) == W_SEED_CST_NONE) return false;
  (void)consume_text(parser, continuation ? "continue" : "break", NULL);
  if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    (void)consume_current(parser, NULL);
  }
  (void)statement_boundary(parser);
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_label_statement(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_LABEL, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_current(parser, NULL);
  if (!expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN)) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    return false;
  }
  if (current_is_text(parser, "repeat")) {
    if (!parse_repeat_statement(parser)) return false;
  } else if (current_is_text(parser, "for")) {
    if (!parse_for_statement(parser)) return false;
  } else if (current_is_text(parser, "{")) {
    if (!parse_block(parser, false)) return false;
  } else {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    return false;
  }
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_expression_statement(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_EXPRESSION_STATEMENT, start) ==
      W_SEED_CST_NONE)
    return false;
  if (!parse_expression(parser, 1, false)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)statement_boundary(parser);
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_expect_statement(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_EXPECT_STATEMENT, start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "expect", NULL);
  if (!parse_expression(parser, 1, true)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)statement_boundary(parser);
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_statement(w_seed_parser *parser) {
  if (!skip_trivia(parser) || current_is_eof(parser)) return false;
  if (current_is_text(parser, "let")) return parse_let_statement(parser);
  if (current_is_text(parser, "var")) return parse_var_statement(parser);
  if (current_is_text(parser, "return")) return parse_return_statement(parser);
  if (current_is_text(parser, "commit")) return parse_commit_statement(parser);
  if (current_is_text(parser, "transaction"))
    return parse_expression_statement(parser);
  if (current_is_text(parser, "if")) return parse_if_statement(parser);
  if (current_is_text(parser, "repeat")) return parse_repeat_statement(parser);
  if (current_is_text(parser, "for")) return parse_for_statement(parser);
  if (current_is_text(parser, "break")) return parse_break_statement(parser, false);
  if (current_is_text(parser, "continue"))
    return parse_break_statement(parser, true);
  if (current_is_text(parser, "expect")) {
    if (!parser->in_test) {
      stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
      return false;
    }
    return parse_expect_statement(parser);
  }
  if (current_is_text(parser, "spawn")) return parse_spawn_statement(parser);
  if (current_is_text(parser, "try") && next_is_text(parser, "allocator")) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    return false;
  }
  if (current_is_text(parser, "allocator")) return parse_allocator_block(parser);
  if (current_is_text(parser, "else") || current_is_text(parser, "catch") ||
      current_is_text(parser, "while")) {
    (void)record_issue(parser, W_SEED_PARSE_ISSUE_NO_CONTINUATION_OWNER,
                       current_span(parser), W_SEED_PARSE_EXPECT_STATEMENT);
    (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR, NULL);
    return true;
  }
  if (current_is_text(parser, "take")) return parse_expression_statement(parser);
  if (is_unsupported_word(parser)) {
    stop_with_remainder(parser,
                        current_is_text(parser, "foreign")
                            ? W_SEED_PARSE_ISSUE_FOREIGN_UNSUPPORTED
                            : W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    return false;
  }
  if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD) &&
      next_is_text(parser, ":")) {
    return parse_label_statement(parser);
  }
  return parse_expression_statement(parser);
}

static bool parse_block(w_seed_parser *parser, bool value_context) {
  if (!current_is_text(parser, "{")) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE);
    return false;
  }
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_BLOCK, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "{", NULL);
  while (!current_is_eof(parser) && !current_is_text(parser, "}")) {
    if (!parse_statement(parser)) {
      if (parser->status == W_SEED_PARSE_FATAL) break;
      if (current_is_eof(parser) || current_is_text(parser, "}")) break;
    }
  }
  if (!expect_text(parser, "}", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  (void)value_context;
  return true;
}

static bool parse_module_path(w_seed_parser *parser) {
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                       current_span(parser), W_SEED_PARSE_EXPECT_WORD);
    return false;
  }
  (void)consume_current(parser, NULL);
  while (current_is_text(parser, ".")) {
    (void)consume_text(parser, ".", NULL);
    if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
      (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                         current_span(parser), W_SEED_PARSE_EXPECT_WORD);
      return false;
    }
    (void)consume_current(parser, NULL);
  }
  return true;
}

static bool parse_import_declaration(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_IMPORT, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "import", NULL);
  if (!current_is_text(parser, "{")) {
    if (current_is_text(parser, "*")) {
      const size_t item_start = current_span(parser).start_byte;
      if (push_node(parser, W_SEED_CST_IMPORT_ITEM, item_start) ==
          W_SEED_CST_NONE)
        return false;
      (void)consume_text(parser, "*", NULL);
      pop_node(parser, parser->last_token_end);
      if (!current_is_text(parser, "from")) {
        append_missing(parser, current_span(parser).start_byte,
                       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
      } else {
        (void)consume_text(parser, "from", NULL);
      }
      if (!parse_module_path(parser)) {
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
      if (current_is_text(parser, ";")) (void)consume_text(parser, ";", NULL);
      pop_node(parser, parser->last_token_end);
      return true;
    }
    if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
      const size_t item_start = current_span(parser).start_byte;
      if (push_node(parser, W_SEED_CST_IMPORT_ITEM, item_start) ==
          W_SEED_CST_NONE)
        return false;
      (void)consume_current(parser, NULL);
      if (current_is_text(parser, ".")) {
        while (current_is_text(parser, ".")) {
          (void)consume_text(parser, ".", NULL);
          if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
            append_missing(parser, current_span(parser).start_byte,
                           W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
            pop_node(parser, parser->last_token_end);
            pop_node(parser, parser->last_token_end);
            return false;
          }
          (void)consume_current(parser, NULL);
        }
        pop_node(parser, parser->last_token_end);
        if (current_is_text(parser, ";")) (void)consume_text(parser, ";", NULL);
        pop_node(parser, parser->last_token_end);
        return true;
      }
      pop_node(parser, parser->last_token_end);
      if (current_is_text(parser, "from")) {
        (void)consume_text(parser, "from", NULL);
        if (!parse_module_path(parser)) {
          pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
          return false;
        }
      } else if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
        /* A second bare word denotes an alias form with its `from` missing. */
        append_missing(parser, current_span(parser).start_byte,
                       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
        if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD) &&
            !parse_module_path(parser)) {
          pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
          return false;
        }
      }
      if (current_is_text(parser, ";")) (void)consume_text(parser, ";", NULL);
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return true;
    }
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)consume_text(parser, "{", NULL);
  if (current_is_text(parser, "}")) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    return false;
  }
  while (true) {
    const size_t item_start = current_span(parser).start_byte;
    if (push_node(parser, W_SEED_CST_IMPORT_ITEM, item_start) ==
        W_SEED_CST_NONE)
      return false;
    if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
      append_missing(parser, current_span(parser).start_byte,
                     W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
      pop_node(parser, parser->last_token_end);
      return false;
    }
    (void)consume_current(parser, NULL);
    pop_node(parser, parser->last_token_end);
    if (!current_is_text(parser, ",")) break;
    (void)consume_text(parser, ",", NULL);
    if (current_is_text(parser, "}")) break;
  }
  if (!expect_text(parser, "}", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (!current_is_text(parser, "from")) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    return false;
  }
  (void)consume_text(parser, "from", NULL);
  if (!parse_module_path(parser)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (current_is_text(parser, ";")) (void)consume_text(parser, ";", NULL);
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_generic_parameters(w_seed_parser *parser,
                                     size_t declaration_end) {
  if (!current_is_text(parser, "<")) return true;
  const size_t start = current_span(parser).start_byte;
  if (start != declaration_end) {
    (void)record_issue(parser, W_SEED_PARSE_ISSUE_SPACED_HEAD,
                       current_span(parser), W_SEED_PARSE_EXPECT_PUNCTUATION);
    return false;
  }
  if (push_node(parser, W_SEED_CST_GENERIC_PARAMETERS, start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "<", NULL);
  if (current_is_text(parser, ">") || current_is_double_gt(parser)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  while (true) {
    const size_t parameter_start = current_span(parser).start_byte;
    if (push_node(parser, W_SEED_CST_GENERIC_PARAMETER, parameter_start) ==
        W_SEED_CST_NONE)
      return false;
    if (current_is_text(parser, "_")) {
      (void)consume_text(parser, "_", NULL);
      if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
        append_missing(parser, current_span(parser).start_byte,
                       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
        pop_node(parser, parser->has_last_token ? parser->last_token_end
                                                : parameter_start);
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
      (void)consume_current(parser, NULL);
      if (!expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
          !parse_type(parser)) {
        pop_node(parser, parser->has_last_token ? parser->last_token_end
                                                : parameter_start);
        pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
        return false;
      }
    } else if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
      (void)consume_current(parser, NULL);
      if (current_is_text(parser, ":")) {
        (void)consume_text(parser, ":", NULL);
        if (!parse_type(parser)) {
          pop_node(parser, parser->has_last_token ? parser->last_token_end
                                                  : parameter_start);
          pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
          return false;
        }
      }
    } else {
      append_missing(parser, current_span(parser).start_byte,
                     W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
      pop_node(parser, parser->has_last_token ? parser->last_token_end
                                              : parameter_start);
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
    pop_node(parser, parser->last_token_end);
    if (!current_is_text(parser, ",")) break;
    (void)consume_text(parser, ",", NULL);
    if (current_is_text(parser, ">") || current_is_double_gt(parser)) break;
  }
  if (current_is_text(parser, ">")) {
    (void)consume_text(parser, ">", NULL);
  } else if (current_is_double_gt(parser)) {
    w_seed_parse_token_view view;
    (void)consume_virtual_close(parser, &view);
  } else {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_field_declaration(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_FIELD, start) == W_SEED_CST_NONE)
    return false;
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (!expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
      !parse_type(parser)) {
    pop_node(parser, parser->last_token_end);
    return false;
  }
  if (current_is_text(parser, ";")) (void)consume_text(parser, ";", NULL);
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_type_or_alias_declaration(w_seed_parser *parser,
                                            bool alias) {
  const size_t start = current_span(parser).start_byte;
  const w_seed_cst_kind kind = alias ? W_SEED_CST_ALIAS_DECLARATION
                                     : W_SEED_CST_TYPE_DECLARATION;
  if (push_node(parser, kind, start) == W_SEED_CST_NONE) return false;
  if (current_is_text(parser, "export")) (void)consume_text(parser, "export", NULL);
  (void)consume_text(parser, alias ? "alias" : "type", NULL);
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (current_is_text(parser, "<") &&
      !parse_generic_parameters(parser, parser->last_token_end)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (!expect_text(parser, "=", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
      !parse_type(parser)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (current_is_text(parser, ";")) (void)consume_text(parser, ";", NULL);
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_struct_declaration(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_STRUCT, start) == W_SEED_CST_NONE)
    return false;
  if (current_is_text(parser, "export")) (void)consume_text(parser, "export", NULL);
  (void)consume_text(parser, "struct", NULL);
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (current_is_text(parser, "<")) {
    if (!parse_generic_parameters(parser, parser->last_token_end)) return false;
  }
  if (current_is_text(parser, ":")) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    return false;
  }
  if (!current_is_text(parser, "{")) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  (void)consume_text(parser, "{", NULL);
  while (!current_is_eof(parser) && !current_is_text(parser, "}")) {
    const size_t before = current_span(parser).start_byte;
    if (!parse_field_declaration(parser)) {
      if (parser->status == W_SEED_PARSE_FATAL || current_is_eof(parser) ||
          current_is_text(parser, "}"))
        break;
      if (current_span(parser).start_byte == before) {
        (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR, NULL);
      }
    }
  }
  if (!expect_text(parser, "}", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

/* Enum payloads use a distinct owner from function parameters.  This keeps
 * declaration order and the positional/named surface visible to the
 * caller-owned frontend without reinterpreting a case as a function. */
static bool parse_enum_case_parameter(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_ENUM_CASE_PARAMETER, start) ==
      W_SEED_CST_NONE)
    return false;
  if (current_is_kind(parser, W_SEED_LEX_ITEM_WORD) &&
      next_is_text(parser, ":")) {
    (void)consume_current(parser, NULL);
    if (!expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
        !parse_type(parser)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  } else if (!parse_type(parser)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_enum_case(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_ENUM_CASE, start) == W_SEED_CST_NONE)
    return false;
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (current_is_text(parser, "(")) {
    const size_t parameter_start = current_span(parser).start_byte;
    (void)consume_text(parser, "(", NULL);
    if (current_is_text(parser, ")")) {
      append_missing(parser, current_span(parser).start_byte,
                     W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    } else {
      while (true) {
        if (!parse_enum_case_parameter(parser)) {
          pop_node(parser, parser->has_last_token ? parser->last_token_end
                                                   : parameter_start);
          return false;
        }
        if (!current_is_text(parser, ",")) break;
        (void)consume_text(parser, ",", NULL);
        if (current_is_text(parser, ")")) break;
      }
    }
    if (!expect_text(parser, ")", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  if (current_is_text(parser, ";"))
    (void)consume_current(parser, NULL);
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool enum_member_keyword(w_seed_parser *parser) {
  static const char *const keywords[] = {
      "export", "static", "fn",       "async", "unsafe", "init",
      "const",  "type",   "alias",    "extension",
      "mut",    "take",   "protocol", "struct",
  };
  for (size_t index = 0; index < sizeof(keywords) / sizeof(keywords[0]);
       index += 1) {
    if (current_is_text(parser, keywords[index])) return true;
  }
  return false;
}

/* Keep unsupported members in the CST as error leaves.  The enum remains
 * deterministic and recovered, but it cannot cross the COMPLETE frontend
 * barrier until a later bundle models members and methods. */
static void recover_enum_member(w_seed_parser *parser) {
  const w_seed_span start = current_span(parser);
  (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM, start, 0);
  size_t brace_depth = 0;
  while (!current_is_eof(parser)) {
    if (current_is_text(parser, "{")) {
      brace_depth += 1;
      (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR, NULL);
      continue;
    }
    if (current_is_text(parser, "}")) {
      if (brace_depth == 0) break;
      brace_depth -= 1;
      (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR, NULL);
      continue;
    }
    const bool semicolon = brace_depth == 0 && current_is_text(parser, ";");
    (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR, NULL);
    if (semicolon) break;
  }
}

static bool parse_enum_declaration(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_ENUM, start) == W_SEED_CST_NONE)
    return false;
  if (current_is_text(parser, "export")) (void)consume_text(parser, "export", NULL);
  (void)consume_text(parser, "enum", NULL);
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (current_is_text(parser, "<") &&
      !parse_generic_parameters(parser, parser->last_token_end)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (current_is_text(parser, ":")) {
    (void)consume_text(parser, ":", NULL);
    if (!parse_type(parser)) {
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
  }
  if (!current_is_text(parser, "{")) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE);
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  (void)consume_text(parser, "{", NULL);
  while (!current_is_eof(parser) && !current_is_text(parser, "}")) {
    const size_t before = current_span(parser).start_byte;
    if (enum_member_keyword(parser)) {
      recover_enum_member(parser);
    } else if (!parse_enum_case(parser)) {
      if (parser->status == W_SEED_PARSE_FATAL || current_is_eof(parser) ||
          current_is_text(parser, "}")) {
        break;
      }
      if (current_span(parser).start_byte == before) {
        (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR,
                           NULL);
      }
    }
  }
  if (!expect_text(parser, "}", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (current_is_text(parser, ";")) (void)consume_text(parser, ";", NULL);
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_literal_events(w_seed_parser *parser) {
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_LITERAL_EVENT)) {
    (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                       current_span(parser), W_SEED_PARSE_EXPECT_EXPRESSION);
    return false;
  }
  do {
    (void)consume_current(parser, NULL);
  } while (current_is_kind(parser, W_SEED_LEX_ITEM_LITERAL_EVENT));
  return true;
}

static bool parse_test_declaration(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_TEST, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "test", NULL);
  if (!parse_literal_events(parser)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  /* The `for WORD` target is optional in this bounded test declaration. */
  if (current_is_text(parser, "for")) {
    (void)consume_text(parser, "for", NULL);
    if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
      append_missing(parser, current_span(parser).start_byte,
                     W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
      pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
      return false;
    }
    (void)consume_current(parser, NULL);
  }
  if (!current_is_text(parser, "{")) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  const bool previous_in_test = parser->in_test;
  parser->in_test = true;
  const bool parsed = parse_block(parser, false);
  parser->in_test = previous_in_test;
  if (!parsed) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_parameter_list(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_PARAMETER_LIST, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "(", NULL);
  if (!current_is_text(parser, ")")) {
    while (true) {
      const size_t parameter_start = current_span(parser).start_byte;
      if (push_node(parser, W_SEED_CST_PARAMETER, parameter_start) ==
          W_SEED_CST_NONE)
        return false;
      if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
        append_missing(parser, current_span(parser).start_byte,
                       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
        pop_node(parser, parser->last_token_end);
        return false;
      }
      w_seed_span first_label;
      (void)consume_current(parser, &first_label);
      const bool first_is_underscore = span_text(parser, first_label, "_");
      if (!current_is_text(parser, ":")) {
        if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
          append_missing(parser, current_span(parser).start_byte,
                         W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
          pop_node(parser, parser->last_token_end);
          return false;
        }
        (void)consume_current(parser, NULL);
      } else if (first_is_underscore) {
        (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                           first_label, W_SEED_PARSE_EXPECT_WORD);
        pop_node(parser, parser->last_token_end);
        return false;
      }
      if (!expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN)) {
        pop_node(parser, parser->last_token_end);
        return false;
      }
      if (current_is_text(parser, "ref") || current_is_text(parser, "inout") ||
          current_is_text(parser, "take") || current_is_text(parser, "const")) {
        (void)consume_current(parser, NULL);
      }
      if (!parse_type(parser)) {
        pop_node(parser, parser->last_token_end);
        return false;
      }
      pop_node(parser, parser->last_token_end);
      if (!current_is_text(parser, ",")) break;
      (void)consume_text(parser, ",", NULL);
      if (current_is_text(parser, ")")) break;
    }
  }
  if (!expect_text(parser, ")", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool foreign_failure(w_seed_parser *parser,
                            const w_seed_foreign_error *scan_error,
                            size_t input_start, size_t remainder_start) {
  size_t primary_offset = input_start;
  size_t primary_end = input_start;
  if (scan_error != NULL &&
      scan_error->primary.start_byte <=
          parser->lexer.bounds.end_byte - input_start) {
    primary_offset = input_start + scan_error->primary.start_byte;
    const size_t relative_end = scan_error->primary.end_byte >=
                                        scan_error->primary.start_byte
                                    ? scan_error->primary.end_byte
                                    : scan_error->primary.start_byte;
    const size_t available = parser->lexer.bounds.end_byte - input_start;
    const size_t clamped_end = relative_end <= available ? relative_end : available;
    primary_end = input_start + clamped_end;
  }
  const w_seed_span primary = {primary_offset, primary_end};
  (void)record_fatal(parser, W_SEED_PARSE_ISSUE_FOREIGN_SCANNER, primary, 0);
  const size_t safe_remainder_start = remainder_start <= parser->lexer.bounds.end_byte
                                          ? remainder_start
                                          : input_start;
  if (safe_remainder_start < parser->lexer.bounds.end_byte) {
    (void)add_raw_span(parser, W_SEED_CST_ERROR, W_SEED_CST_FLAG_ERROR,
                       (w_seed_span){safe_remainder_start,
                                     parser->lexer.bounds.end_byte});
  }
  parser->token_count = 0;
  parser->lexer.cursor = parser->lexer.bounds.end_byte;
  parser->lexer.terminal = true;
  return false;
}

static bool parse_foreign_language_tag(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (start != parser->last_token_end) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_FOREIGN_UNSUPPORTED);
    return false;
  }
  if (push_node(parser, W_SEED_CST_FOREIGN_LANGUAGE_TAG, start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "<", NULL);
  bool accepted = false;
  if (current_is_text(parser, "C")) {
    (void)consume_text(parser, "C", NULL);
    accepted = true;
  } else if (current_is_text(parser, "lang")) {
    (void)consume_text(parser, "lang", NULL);
    if (expect_text(parser, ":", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) &&
        expect_text(parser, ".", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) &&
        current_is_text(parser, "c")) {
      (void)consume_text(parser, "c", NULL);
      accepted = true;
    }
  }
  if (!accepted) {
    stop_with_remainder(parser, W_SEED_PARSE_ISSUE_FOREIGN_UNSUPPORTED);
    pop_node(parser, parser->lexer.bounds.end_byte);
    return false;
  }
  if (!expect_text(parser, ">", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_foreign_body(w_seed_parser *parser) {
  if (!current_is_text(parser, "{")) return false;
  const size_t opening_start = current_span(parser).start_byte;
  const size_t body_start = opening_start + 1;
  if (push_node(parser, W_SEED_CST_FOREIGN_BODY_OWNER, opening_start) ==
      W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "{", NULL);
  if (parser->token_count != 0 || body_start > parser->lexer.bounds.end_byte) {
    const w_seed_foreign_error error = {
        W_SEED_FOREIGN_ERROR_NULL_ARGUMENT,
        W_SEED_FOREIGN_TERMINAL_NONE,
        {body_start, body_start},
        {opening_start, body_start},
        false,
        0,
    };
    const bool failed = foreign_failure(parser, &error, opening_start, body_start);
    pop_node(parser, parser->lexer.bounds.end_byte);
    return failed;
  }

  const w_seed_byte_view input = {
      parser->source->bytes.data + opening_start,
      parser->lexer.bounds.end_byte - opening_start,
  };
  w_seed_foreign_source_validation scan;
  w_seed_foreign_error scan_error;
  if (!w_seed_foreign_scan_c_inline_1(input, parser->foreign_limits, &scan,
                                      &scan_error)) {
    const bool failed = foreign_failure(parser, &scan_error, opening_start, body_start);
    pop_node(parser, parser->lexer.bounds.end_byte);
    return failed;
  }
  if (scan.body_start_byte != 1 ||
      scan.body_end_byte < scan.body_start_byte ||
      scan.close_byte != scan.body_end_byte || scan.next_byte <= scan.close_byte ||
      scan.next_byte > input.length || !scan.digest_valid ||
      scan.body_start_byte > input.length ||
      scan.body_end_byte > input.length) {
    const w_seed_foreign_error error = {
        W_SEED_FOREIGN_ERROR_MISSING_CLOSE,
        W_SEED_FOREIGN_TERMINAL_MISSING_CLOSE,
        {0, 0},
        {0, 1},
        false,
        0,
    };
    const bool failed = foreign_failure(parser, &error, opening_start, body_start);
    pop_node(parser, parser->lexer.bounds.end_byte);
    return failed;
  }
  const size_t absolute_body_start = opening_start + scan.body_start_byte;
  const size_t absolute_body_end = opening_start + scan.body_end_byte;
  const size_t absolute_close = opening_start + scan.close_byte;
  if (absolute_body_start != parser->lexer.cursor ||
      absolute_close >= parser->source->bytes.length ||
      parser->source->bytes.data[absolute_close] != (uint8_t)'}') {
    const w_seed_foreign_error error = {
        W_SEED_FOREIGN_ERROR_MISSING_CLOSE,
        W_SEED_FOREIGN_TERMINAL_MISSING_CLOSE,
        {absolute_close, absolute_close},
        {opening_start, body_start},
        false,
        0,
    };
    const bool failed = foreign_failure(parser, &error, opening_start, body_start);
    pop_node(parser, parser->lexer.bounds.end_byte);
    return failed;
  }
  w_seed_lex_error lex_error;
  if (!w_seed_lexer_require_opaque(&parser->lexer, &lex_error) ||
      !w_seed_lexer_claim_opaque(
          &parser->lexer, (w_seed_span){absolute_body_start, absolute_body_end},
          &lex_error)) {
    const w_seed_foreign_error error = {
        W_SEED_FOREIGN_ERROR_MISSING_CLOSE,
        W_SEED_FOREIGN_TERMINAL_MISSING_CLOSE,
        lex_error.primary,
        lex_error.opening,
        false,
        0,
    };
    const bool failed = foreign_failure(parser, &error, opening_start, body_start);
    pop_node(parser, parser->lexer.bounds.end_byte);
    return failed;
  }
  if (!consume_raw(parser, 0, W_SEED_CST_FOREIGN_BODY, NULL) ||
      !expect_text(parser, "}", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end
                                            : parser->lexer.bounds.end_byte);
    return false;
  }
  pop_node(parser, parser->last_token_end);
  return true;
}

static bool parse_function(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_FUNCTION, start) == W_SEED_CST_NONE)
    return false;
  if (current_is_text(parser, "export"))
    (void)consume_text(parser, "export", NULL);
  bool unsafe_prefix = false;
  if (current_is_text(parser, "unsafe")) {
    unsafe_prefix = true;
    (void)consume_text(parser, "unsafe", NULL);
  }
  if (current_is_text(parser, "async"))
    (void)consume_text(parser, "async", NULL);
  (void)consume_text(parser, "fn", NULL);
  bool abi_contract = false;
  bool foreign = false;
  if (current_is_text(parser, "<") && parser->last_token_end ==
                                             current_span(parser).start_byte &&
      next_is_text(parser, "abi")) {
    abi_contract = parse_contract_envelope(parser, parser->last_token_end, false);
  } else if (current_is_text(parser, "<")) {
    foreign = parse_foreign_language_tag(parser);
  }
  if (parser->status == W_SEED_PARSE_FATAL) {
    pop_node(parser, parser->lexer.bounds.end_byte);
    return false;
  }
  if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (current_is_text(parser, "<") &&
      !parse_generic_parameters(parser, parser->last_token_end)) {
    pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
    return false;
  }
  if (!current_is_text(parser, "(")) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  if (!parse_parameter_list(parser)) return false;
  if (current_is_text(parser, ":")) {
    const size_t type_start = current_span(parser).start_byte;
    if (push_node(parser, W_SEED_CST_RETURN_TYPE, type_start) ==
        W_SEED_CST_NONE)
      return false;
    (void)consume_text(parser, ":", NULL);
    if (!parse_type(parser)) return false;
    pop_node(parser, parser->last_token_end);
  }
  if (current_is_text(parser, "throws")) {
    (void)consume_text(parser, "throws", NULL);
    if (!parse_type(parser)) return false;
  }
  if (current_is_text(parser, "borrows") && !parse_borrow_clause(parser))
    return false;
  if (foreign) {
    if (!parse_foreign_body(parser)) return false;
  } else {
    if (unsafe_prefix && !abi_contract) {
      stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
      pop_node(parser, parser->lexer.bounds.end_byte);
      return false;
    }
    if (!parse_block(parser, false)) return false;
  }
  pop_node(parser, parser->has_last_token ? parser->last_token_end : start);
  return true;
}

static bool parse_entry(w_seed_parser *parser) {
  const size_t start = current_span(parser).start_byte;
  if (push_node(parser, W_SEED_CST_ENTRY, start) == W_SEED_CST_NONE)
    return false;
  (void)consume_text(parser, "entry", NULL);
  if (!expect_text(parser, "(", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN) ||
      !current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
    append_missing(parser, current_span(parser).start_byte,
                   W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
    pop_node(parser, parser->last_token_end);
    return false;
  }
  (void)consume_current(parser, NULL);
  if (!expect_text(parser, ")", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE)) {
    pop_node(parser, parser->last_token_end);
    return false;
  }
  if (current_is_text(parser, ";")) (void)consume_text(parser, ";", NULL);
  pop_node(parser, parser->last_token_end);
  return true;
}

static void unwind_frames(w_seed_parser *parser, size_t end) {
  while (parser->frame_count != 0) pop_node(parser, end);
}

bool w_seed_parser_init(const w_seed_source *source, w_seed_span bounds,
                        w_seed_foreign_limits foreign_limits,
                        w_seed_lexer_frame *lexer_frames,
                        size_t lexer_frame_capacity,
                        w_seed_parse_token *token_cache,
                        size_t token_capacity, w_seed_cst_node *nodes,
                        size_t node_capacity, w_seed_parse_frame *frames,
                        size_t frame_capacity, w_seed_parse_issue *issues,
                        size_t issue_capacity, w_seed_parser *parser,
                        w_seed_lex_error *lex_error) {
  if (!source_ready(source) || parser == NULL ||
      (lexer_frame_capacity != 0 && lexer_frames == NULL) ||
      (token_capacity != 0 && token_cache == NULL) ||
      (node_capacity != 0 && nodes == NULL) ||
      (frame_capacity != 0 && frames == NULL) ||
      (issue_capacity != 0 && issues == NULL)) {
    return false;
  }
  (void)memset(parser, 0, sizeof(*parser));
  parser->source = source;
  parser->foreign_limits = foreign_limits;
  parser->lexer_frames = lexer_frames;
  parser->lexer_frame_capacity = lexer_frame_capacity;
  parser->token_cache = token_cache;
  parser->token_capacity = token_capacity;
  parser->nodes = nodes;
  parser->node_capacity = node_capacity;
  parser->frames = frames;
  parser->frame_capacity = frame_capacity;
  parser->issues = issues;
  parser->issue_capacity = issue_capacity;
  parser->status = W_SEED_PARSE_COMPLETE;
  parser->root = W_SEED_CST_NONE;
  if (!w_seed_lexer_init(source, bounds, lexer_frames, lexer_frame_capacity,
                         &parser->lexer, lex_error)) {
    return false;
  }
  return true;
}

bool w_seed_parser_parse(w_seed_parser *parser, w_seed_parse_result *result) {
  if (parser == NULL || result == NULL || parser->source == NULL ||
      parser->parsed)
    return false;
  parser->parsed = true;
  parser->status = W_SEED_PARSE_COMPLETE;
  parser->root = push_node(parser, W_SEED_CST_DOCUMENT,
                           parser->lexer.bounds.start_byte);
  if (parser->root == W_SEED_CST_NONE) return false;
  bool saw_module = false;
  bool saw_entry = false;
  bool saw_declaration = false;
  parser->imports_allowed = true;
  if (current_is_text(parser, "module")) {
    saw_module = true;
    const size_t start = current_span(parser).start_byte;
    if (push_node(parser, W_SEED_CST_MODULE_HEADER, start) == W_SEED_CST_NONE)
      return false;
    (void)consume_text(parser, "module", NULL);
    if (!current_is_kind(parser, W_SEED_LEX_ITEM_WORD)) {
      append_missing(parser, current_span(parser).start_byte,
                     W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN);
      pop_node(parser, parser->last_token_end);
      unwind_frames(parser, parser->lexer.bounds.end_byte);
      goto done;
    }
    (void)consume_current(parser, NULL);
    if (current_is_text(parser, ";")) (void)consume_text(parser, ";", NULL);
    pop_node(parser, parser->last_token_end);
  }
  while (!current_is_eof(parser) && parser->status != W_SEED_PARSE_FATAL) {
    if (current_is_text(parser, "import")) {
      if (!parser->imports_allowed) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      if (!parse_import_declaration(parser)) break;
      continue;
    }
    if (current_is_text(parser, "export")) {
      if (!next_is_text(parser, "fn") &&
          !next_two_are_text(parser, "async", "fn") &&
          !next_two_are_text(parser, "unsafe", "fn") &&
          !next_is_text(parser, "struct") && !next_is_text(parser, "type") &&
          !next_is_text(parser, "alias") && !next_is_text(parser, "enum")) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      if (saw_entry) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      parser->imports_allowed = false;
      saw_declaration = true;
      if (next_is_text(parser, "fn") ||
          next_two_are_text(parser, "async", "fn") ||
          next_two_are_text(parser, "unsafe", "fn")) {
        if (!parse_function(parser)) break;
      } else if (next_is_text(parser, "type")) {
        if (!parse_type_or_alias_declaration(parser, false)) break;
      } else if (next_is_text(parser, "alias")) {
        if (!parse_type_or_alias_declaration(parser, true)) break;
      } else if (next_is_text(parser, "enum")) {
        if (!parse_enum_declaration(parser)) break;
      } else if (!parse_struct_declaration(parser)) {
        break;
      }
      continue;
    }
    if (current_is_text(parser, "unsafe")) {
      if (!next_is_text(parser, "fn")) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      if (saw_entry) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      parser->imports_allowed = false;
      saw_declaration = true;
      if (!parse_function(parser)) break;
      continue;
    }
    if (current_is_text(parser, "async")) {
      if (!next_is_text(parser, "fn")) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      if (saw_entry) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      parser->imports_allowed = false;
      saw_declaration = true;
      if (!parse_function(parser)) break;
      continue;
    }
    if (current_is_text(parser, "fn")) {
      if (saw_entry) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      parser->imports_allowed = false;
      saw_declaration = true;
      if (!parse_function(parser)) break;
      continue;
    }
    if (current_is_text(parser, "entry")) {
      if (saw_entry) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      parser->imports_allowed = false;
      saw_declaration = true;
      saw_entry = true;
      if (!parse_entry(parser)) break;
      continue;
    }
    if (current_is_text(parser, "struct")) {
      if (saw_entry) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      parser->imports_allowed = false;
      saw_declaration = true;
      if (!parse_struct_declaration(parser)) break;
      continue;
    }
    if (current_is_text(parser, "enum")) {
      if (saw_entry) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      parser->imports_allowed = false;
      saw_declaration = true;
      if (!parse_enum_declaration(parser)) break;
      continue;
    }
    if (current_is_text(parser, "type")) {
      if (saw_entry) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      parser->imports_allowed = false;
      saw_declaration = true;
      if (!parse_type_or_alias_declaration(parser, false)) break;
      continue;
    }
    if (current_is_text(parser, "alias")) {
      if (saw_entry) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      parser->imports_allowed = false;
      saw_declaration = true;
      if (!parse_type_or_alias_declaration(parser, true)) break;
      continue;
    }
    if (current_is_text(parser, "test")) {
      if (saw_entry) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
        break;
      }
      parser->imports_allowed = false;
      saw_declaration = true;
      if (!parse_test_declaration(parser)) break;
      continue;
    }
    if (current_is_text(parser, "expect")) {
      stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
      break;
    }
    if (current_is_text(parser, "try") && next_is_text(parser, "allocator")) {
      stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
      break;
    }
    if (current_is_text(parser, "package") || current_is_text(parser, "workspace")) {
      if (saw_module || saw_declaration) {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_MIXED_ROOT);
      } else {
        stop_with_remainder(parser, W_SEED_PARSE_ISSUE_UNSUPPORTED_ROOT);
      }
      break;
    }
    if (is_unsupported_word(parser)) {
      stop_with_remainder(parser, current_is_text(parser, "foreign")
                                          ? W_SEED_PARSE_ISSUE_FOREIGN_UNSUPPORTED
                                          : W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
      break;
    }
    (void)record_issue(parser, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN,
                       current_span(parser), W_SEED_PARSE_EXPECT_STATEMENT);
    (void)consume_raw(parser, W_SEED_CST_FLAG_ERROR, W_SEED_CST_ERROR, NULL);
  }
  (void)saw_module;
done:
  if (parser->status != W_SEED_PARSE_FATAL) {
    (void)skip_trivia(parser);
    if (!current_is_eof(parser)) {
      preserve_remainder(parser);
    }
  }
  unwind_frames(parser, parser->lexer.bounds.end_byte);
  result->status = parser->status;
  result->root = parser->root;
  result->node_count = parser->node_count;
  result->leaf_count = parser->leaf_count;
  result->issue_count = parser->issue_count;
  result->consumed_byte = parser->lexer.bounds.end_byte;
  return true;
}
