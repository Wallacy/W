#include "w_seed_module_scan.h"

#include <limits.h>
#include <string.h>

typedef struct {
  const w_seed_source *source;
  const w_seed_cst_node *nodes;
  size_t node_count;
  w_seed_span bounds;
  size_t next_node;
} module_scan_tokens;

typedef struct {
  w_seed_cst_kind kind;
  w_seed_span span;
} module_scan_token;

static w_seed_span empty_span(size_t offset) {
  const w_seed_span span = {offset, offset};
  return span;
}

static bool source_span_valid(const w_seed_source *source, w_seed_span span) {
  if (source == NULL) return false;
  return w_seed_source_validate_span(source, span, NULL);
}

static bool node_is_raw(const w_seed_cst_node *node) {
  return node != NULL && (node->flags & W_SEED_CST_FLAG_RAW_LEAF) != 0u;
}

static bool node_is_trivia(const w_seed_cst_node *node) {
  return node != NULL &&
         (node->kind == W_SEED_CST_SOURCE_PREFIX ||
          node->kind == W_SEED_CST_TRIVIA ||
          (node->flags & W_SEED_CST_FLAG_TRIVIA) != 0u);
}

static bool token_next(module_scan_tokens *tokens, module_scan_token *token) {
  if (tokens == NULL || token == NULL || tokens->nodes == NULL) return false;
  while (tokens->next_node < tokens->node_count) {
    const w_seed_cst_node *node = &tokens->nodes[tokens->next_node];
    tokens->next_node += 1u;
    if (!node_is_raw(node) || node_is_trivia(node)) continue;
    if (node->raw_span.start_byte < tokens->bounds.start_byte ||
        node->raw_span.end_byte > tokens->bounds.end_byte) {
      continue;
    }
    token->kind = node->kind;
    token->span = node->raw_span;
    return true;
  }
  return false;
}

static bool token_is_text(const w_seed_source *source,
                          const module_scan_token *token, const char *text) {
  if (source == NULL || token == NULL || text == NULL ||
      !source_span_valid(source, token->span)) {
    return false;
  }
  const size_t length = strlen(text);
  return token->kind == W_SEED_CST_WORD || token->kind == W_SEED_CST_PUNCTUATION
             ? token->span.end_byte - token->span.start_byte == length &&
                   (length == 0u ||
                    memcmp(source->bytes.data + token->span.start_byte, text,
                           length) == 0)
             : false;
}

static bool token_is_word(const module_scan_token *token) {
  return token != NULL && token->kind == W_SEED_CST_WORD;
}

static bool next_expected(module_scan_tokens *tokens,
                          const w_seed_source *source, const char *text,
                          module_scan_token *taken) {
  module_scan_token token;
  if (!token_next(tokens, &token) || !token_is_text(source, &token, text)) {
    return false;
  }
  if (taken != NULL) *taken = token;
  return true;
}

static bool parse_path(module_scan_tokens *tokens, const w_seed_source *source,
                       w_seed_span *path_span) {
  if (tokens == NULL || source == NULL || path_span == NULL) return false;
  module_scan_token token;
  if (!token_next(tokens, &token) || !token_is_word(&token)) return false;
  w_seed_span path = token.span;
  while (true) {
    module_scan_tokens lookahead = *tokens;
    module_scan_token dot;
    if (!token_next(&lookahead, &dot)) break;
    if (!token_is_text(source, &dot, ".")) break;
    module_scan_token segment;
    if (!token_next(&lookahead, &segment) || !token_is_word(&segment)) {
      return false;
    }
    *tokens = lookahead;
    path.end_byte = segment.span.end_byte;
  }
  *path_span = path;
  return true;
}

static bool parse_import_path(const w_seed_source *source,
                              const w_seed_cst_node *nodes, size_t node_count,
                              w_seed_span declaration_span,
                              w_seed_span *path_span) {
  if (path_span != NULL) *path_span = empty_span(declaration_span.start_byte);
  if (source == NULL || nodes == NULL || path_span == NULL || node_count == 0u ||
      !source_span_valid(source, declaration_span)) {
    return false;
  }
  module_scan_tokens tokens = {source, nodes, node_count, declaration_span, 0u};
  module_scan_token token;
  if (!next_expected(&tokens, source, "import", &token)) return false;

  if (!token_next(&tokens, &token)) return false;
  if (token_is_text(source, &token, "{")) {
    bool need_name = true;
    bool saw_name = false;
    while (true) {
      if (!token_next(&tokens, &token)) return false;
      if (token_is_text(source, &token, "}")) {
        if (need_name || !saw_name) return false;
        break;
      }
      if (!need_name || !token_is_word(&token)) return false;
      saw_name = true;
      need_name = false;
      module_scan_tokens lookahead = tokens;
      module_scan_token separator;
      if (!token_next(&lookahead, &separator)) return false;
      if (token_is_text(source, &separator, ",")) {
        tokens = lookahead;
        need_name = true;
        continue;
      }
      if (token_is_text(source, &separator, "}")) {
        tokens = lookahead;
        break;
      }
      return false;
    }
    if (!next_expected(&tokens, source, "from", NULL) ||
        !parse_path(&tokens, source, path_span)) {
      return false;
    }
  } else if (token_is_text(source, &token, "*")) {
    if (!next_expected(&tokens, source, "from", NULL) ||
        !parse_path(&tokens, source, path_span)) {
      return false;
    }
  } else if (token_is_word(&token)) {
    module_scan_tokens lookahead = tokens;
    module_scan_token next;
    if (token_next(&lookahead, &next) &&
        token_is_text(source, &next, "from")) {
      tokens = lookahead;
      if (!parse_path(&tokens, source, path_span)) return false;
    } else {
      w_seed_span path = token.span;
      tokens = (module_scan_tokens){source, nodes, node_count,
                                   declaration_span, tokens.next_node};
      while (true) {
        module_scan_tokens path_lookahead = tokens;
        module_scan_token dot;
        if (!token_next(&path_lookahead, &dot)) break;
        if (!token_is_text(source, &dot, ".")) break;
        module_scan_token segment;
        if (!token_next(&path_lookahead, &segment) ||
            !token_is_word(&segment)) {
          return false;
        }
        tokens = path_lookahead;
        path.end_byte = segment.span.end_byte;
      }
      *path_span = path;
    }
  } else {
    return false;
  }

  if (token_next(&tokens, &token)) {
    if (!token_is_text(source, &token, ";") || token_next(&tokens, &token)) {
      return false;
    }
  }
  return source_span_valid(source, *path_span) &&
         path_span->start_byte < path_span->end_byte;
}

static bool direct_child_next(const w_seed_cst_node *nodes, size_t node_count,
                              uint32_t *cursor, uint32_t *child) {
  if (nodes == NULL || cursor == NULL || child == NULL ||
      *cursor == W_SEED_CST_NONE || *cursor >= node_count) {
    return false;
  }
  *child = *cursor;
  *cursor = nodes[*cursor].next_sibling;
  return true;
}

static bool validate_nodes(const w_seed_source *source,
                           const w_seed_cst_node *nodes, size_t node_count,
                           const w_seed_parse_result *parse) {
  if (source == NULL || nodes == NULL || parse == NULL || node_count == 0u ||
      node_count != parse->node_count || node_count > (size_t)UINT32_MAX ||
      parse->root == W_SEED_CST_NONE || parse->root >= node_count ||
      parse->leaf_count > node_count || parse->consumed_byte > source->bytes.length ||
      nodes[parse->root].kind != W_SEED_CST_DOCUMENT) {
    return false;
  }
  for (size_t index = 0u; index < node_count; index += 1u) {
    const w_seed_cst_node *node = &nodes[index];
    if (!source_span_valid(source, node->raw_span)) return false;
    if (node->first_child != W_SEED_CST_NONE && node->first_child >= node_count)
      return false;
    if (node->next_sibling != W_SEED_CST_NONE &&
        node->next_sibling >= node_count)
      return false;
  }
  for (size_t index = 0u; index < node_count; index += 1u) {
    const w_seed_cst_node *owner = &nodes[index];
    uint32_t cursor = owner->first_child;
    size_t guard = 0u;
    size_t previous_start = owner->raw_span.start_byte;
    while (cursor != W_SEED_CST_NONE) {
      if (guard >= node_count || cursor >= node_count || cursor == index ||
          nodes[cursor].raw_span.start_byte < previous_start ||
          nodes[cursor].raw_span.start_byte < owner->raw_span.start_byte ||
          nodes[cursor].raw_span.end_byte > owner->raw_span.end_byte) {
        return false;
      }
      previous_start = nodes[cursor].raw_span.start_byte;
      cursor = nodes[cursor].next_sibling;
      guard += 1u;
    }
  }
  return true;
}

static bool header_name_span(const w_seed_source *source,
                             const w_seed_cst_node *nodes, size_t node_count,
                             w_seed_span declaration_span,
                             w_seed_span *name_span) {
  if (name_span != NULL) *name_span = empty_span(declaration_span.start_byte);
  if (source == NULL || nodes == NULL || name_span == NULL ||
      !source_span_valid(source, declaration_span)) {
    return false;
  }
  module_scan_tokens tokens = {source, nodes, node_count, declaration_span, 0u};
  module_scan_token token;
  if (!next_expected(&tokens, source, "module", &token) ||
      !token_next(&tokens, &token) || !token_is_word(&token)) {
    return false;
  }
  *name_span = token.span;
  if (token_next(&tokens, &token)) {
    if (!token_is_text(source, &token, ";") || token_next(&tokens, &token)) {
      return false;
    }
  }
  return source_span_valid(source, *name_span) &&
         name_span->start_byte < name_span->end_byte;
}

bool w_seed_module_scan_import_path_span(
    const w_seed_source *source, const w_seed_cst_node *nodes,
    size_t node_count, w_seed_span declaration_span, w_seed_span *path_span) {
  return parse_import_path(source, nodes, node_count, declaration_span,
                           path_span);
}

w_seed_module_scan_status w_seed_module_scan(
    const w_seed_source *source, const w_seed_cst_node *nodes,
    size_t node_count, const w_seed_parse_result *parse,
    w_seed_module_origin *origins, size_t origin_capacity,
    w_seed_module_scan_result *result) {
  if (result == NULL) return W_SEED_MODULE_SCAN_INVALID;
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_MODULE_SCAN_INVALID;
  result->module_header_name_span = empty_span(0u);
  if (source == NULL || nodes == NULL || parse == NULL ||
      !validate_nodes(source, nodes, node_count, parse) ||
      parse->status != W_SEED_PARSE_COMPLETE || parse->issue_count != 0u) {
    return result->status;
  }
  uint32_t cursor = nodes[parse->root].first_child;
  uint32_t child = W_SEED_CST_NONE;
  size_t guard = 0u;
  size_t previous_start = nodes[parse->root].raw_span.start_byte;
  size_t previous_import_start = 0u;
  bool saw_import = false;
  size_t import_count = 0u;
  while (direct_child_next(nodes, node_count, &cursor, &child)) {
    if (guard >= node_count || nodes[child].raw_span.start_byte < previous_start) {
      result->status = W_SEED_MODULE_SCAN_INVALID;
      return result->status;
    }
    previous_start = nodes[child].raw_span.start_byte;
    if (nodes[child].kind == W_SEED_CST_MODULE_HEADER) {
      if (result->has_module_header_name ||
          !header_name_span(source, nodes, node_count, nodes[child].raw_span,
                            &result->module_header_name_span)) {
        result->status = W_SEED_MODULE_SCAN_UNSUPPORTED;
        return result->status;
      }
      result->has_module_header_name = true;
    } else if (nodes[child].kind == W_SEED_CST_IMPORT) {
      if (saw_import &&
          nodes[child].raw_span.start_byte <= previous_import_start) {
        result->status = W_SEED_MODULE_SCAN_INVALID;
        return result->status;
      }
      w_seed_span path = empty_span(nodes[child].raw_span.start_byte);
      if (!parse_import_path(source, nodes, node_count,
                             nodes[child].raw_span, &path)) {
        result->status = W_SEED_MODULE_SCAN_UNSUPPORTED;
        return result->status;
      }
      previous_import_start = nodes[child].raw_span.start_byte;
      saw_import = true;
      if (import_count == SIZE_MAX) {
        result->status = W_SEED_MODULE_SCAN_INVALID;
        return result->status;
      }
      import_count += 1u;
    }
    guard += 1u;
  }
  result->required = import_count;
  if (import_count != 0u &&
      (origins == NULL || origin_capacity < import_count)) {
    result->status = W_SEED_MODULE_SCAN_CAPACITY;
    return result->status;
  }
  cursor = nodes[parse->root].first_child;
  child = W_SEED_CST_NONE;
  guard = 0u;
  size_t ordinal = 0u;
  while (direct_child_next(nodes, node_count, &cursor, &child)) {
    if (nodes[child].kind != W_SEED_CST_IMPORT) {
      guard += 1u;
      continue;
    }
    w_seed_span path = empty_span(nodes[child].raw_span.start_byte);
    if (!parse_import_path(source, nodes, node_count, nodes[child].raw_span,
                           &path)) {
      result->status = W_SEED_MODULE_SCAN_UNSUPPORTED;
      result->written = 0u;
      return result->status;
    }
    origins[ordinal] = (w_seed_module_origin){
        W_SEED_MODULE_ORIGIN_IMPORT,
        (uint32_t)ordinal,
        child,
        nodes[child].raw_span,
        path,
    };
    ordinal += 1u;
    guard += 1u;
  }
  result->written = ordinal;
  result->status = W_SEED_MODULE_SCAN_OK;
  return result->status;
}
