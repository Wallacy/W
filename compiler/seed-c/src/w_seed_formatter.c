#include "w_seed_formatter.h"

#include <limits.h>
#include <string.h>

enum {
  FMT_TOKEN_TRIVIA = 1,
  FMT_TOKEN_COMMENT = 2,
  FMT_TOKEN_SIGNIFICANT = 3,
  FMT_TOKEN_FOREIGN = 4,
  FMT_TOKEN_FLAG_LITERAL = 1u << 4,
  FMT_PREFERRED_COLUMNS = 120,
};

typedef struct {
  const w_seed_parser *parser;
  const w_seed_formatter_buffers *buffers;
  uint8_t *output;
  size_t capacity;
  size_t required;
  size_t written;
  bool line_start;
  uint8_t last;
  size_t column;
  size_t previous_significant;
  bool have_previous;
  size_t active_multiline_blocks;
  size_t render_indent;
} fmt_writer;

static bool checked_add_size(size_t *value, size_t amount) {
  if (*value > SIZE_MAX - amount) {
    *value = SIZE_MAX;
    return false;
  }
  *value += amount;
  return true;
}

static bool valid_span(const w_seed_parser *parser, w_seed_span span) {
  return span.start_byte <= span.end_byte &&
         span.end_byte <= parser->source->bytes.length;
}

static const uint8_t *source_bytes(const fmt_writer *writer) {
  return writer->parser->source->bytes.data;
}

static size_t token_length(const fmt_writer *writer, size_t index) {
  return writer->buffers->tokens[index].span.end_byte -
         writer->buffers->tokens[index].span.start_byte;
}

static bool token_text(const fmt_writer *writer, size_t index, const char *text) {
  const w_seed_span span = writer->buffers->tokens[index].span;
  const size_t length = strlen(text);
  if (span.end_byte < span.start_byte || span.end_byte >
      writer->parser->source->bytes.length ||
      span.end_byte - span.start_byte != length) {
    return false;
  }
  return length == 0 || memcmp(source_bytes(writer) + span.start_byte, text,
                               length) == 0;
}

static bool span_contains_newline(const fmt_writer *writer, w_seed_span span) {
  for (size_t offset = span.start_byte; offset < span.end_byte; offset += 1) {
    if (source_bytes(writer)[offset] == (uint8_t)'\n') return true;
  }
  return false;
}

static bool span_starts(const fmt_writer *writer, w_seed_span span,
                        const char *text) {
  const size_t length = strlen(text);
  return span.end_byte >= span.start_byte &&
         span.end_byte - span.start_byte >= length &&
         span.start_byte <= writer->parser->source->bytes.length &&
         length <= writer->parser->source->bytes.length - span.start_byte &&
         memcmp(source_bytes(writer) + span.start_byte, text, length) == 0;
}

static bool is_comment_token(const fmt_writer *writer, w_seed_span span) {
  return span_starts(writer, span, "//") || span_starts(writer, span, "/*");
}

static bool is_open_char(uint8_t value) {
  return value == (uint8_t)'(' || value == (uint8_t)'[' ||
         value == (uint8_t)'{';
}

static bool is_close_char(uint8_t value) {
  return value == (uint8_t)')' || value == (uint8_t)']' ||
         value == (uint8_t)'}';
}

static char close_for(char open) {
  switch (open) {
    case '(':
      return ')';
    case '[':
      return ']';
    case '{':
      return '}';
    default:
      return '\0';
  }
}

static bool node_is_statement(w_seed_cst_kind kind) {
  switch (kind) {
    case W_SEED_CST_LET_STATEMENT:
    case W_SEED_CST_VAR_STATEMENT:
    case W_SEED_CST_RETURN_STATEMENT:
    case W_SEED_CST_IF_STATEMENT:
    case W_SEED_CST_REPEAT_STATEMENT:
    case W_SEED_CST_FOR_STATEMENT:
    case W_SEED_CST_BREAK_STATEMENT:
    case W_SEED_CST_CONTINUE_STATEMENT:
    case W_SEED_CST_EXPRESSION_STATEMENT:
    case W_SEED_CST_EXPECT_STATEMENT:
    case W_SEED_CST_COMMIT_STATEMENT:
    case W_SEED_CST_SPAWN_STATEMENT:
    case W_SEED_CST_ALLOCATOR_BLOCK:
    case W_SEED_CST_LABEL:
      return true;
    default:
      return false;
  }
}

static bool parser_is_clean(const w_seed_parser *parser) {
  if (parser == NULL || parser->source == NULL || !parser->parsed ||
      parser->status != W_SEED_PARSE_COMPLETE ||
      parser->root == W_SEED_CST_NONE || parser->nodes == NULL) {
    return false;
  }
  for (size_t index = 0; index < parser->node_count; index += 1) {
    const w_seed_cst_node *node = &parser->nodes[index];
    if ((node->flags & (W_SEED_CST_FLAG_ERROR | W_SEED_CST_FLAG_MISSING)) != 0 ||
        !valid_span(parser, node->raw_span)) {
      return false;
    }
  }
  return true;
}

static bool append_token(const w_seed_parser *parser,
                         const w_seed_cst_node *node,
                         w_seed_format_token *token) {
  if ((node->flags & W_SEED_CST_FLAG_RAW_LEAF) == 0 ||
      node->kind == W_SEED_CST_SOURCE_PREFIX ||
      node->raw_span.start_byte == node->raw_span.end_byte) {
    return false;
  }
  token->span = node->raw_span;
  token->flags = 0;
  if ((node->flags & W_SEED_CST_FLAG_TRIVIA) != 0) {
    token->kind = is_comment_token(
                      &(fmt_writer){.parser = parser}, node->raw_span)
                      ? FMT_TOKEN_COMMENT
                      : FMT_TOKEN_TRIVIA;
  } else if (node->kind == W_SEED_CST_FOREIGN_BODY) {
    token->kind = FMT_TOKEN_FOREIGN;
  } else {
    token->kind = FMT_TOKEN_SIGNIFICANT;
    if (node->kind == W_SEED_CST_LITERAL_EVENT) {
      token->flags = FMT_TOKEN_FLAG_LITERAL;
    }
  }
  return true;
}

static bool build_tokens(const w_seed_parser *parser,
                         const w_seed_formatter_buffers *buffers,
                         size_t *count_out) {
  if (buffers->tokens == NULL || buffers->token_capacity == 0) return false;
  size_t count = 0;
  for (size_t index = 0; index < parser->node_count; index += 1) {
    w_seed_format_token token;
    if (!append_token(parser, &parser->nodes[index], &token)) continue;
    if (count >= buffers->token_capacity) return false;
    buffers->tokens[count] = token;
    count += 1;
  }
  *count_out = count;
  return true;
}

static bool token_is_significant(const w_seed_formatter_buffers *buffers,
                                 size_t index) {
  return buffers->tokens[index].kind == FMT_TOKEN_SIGNIFICANT ||
         buffers->tokens[index].kind == FMT_TOKEN_FOREIGN;
}

static size_t previous_significant(const w_seed_formatter_buffers *buffers,
                                   size_t index) {
  while (index != 0) {
    index -= 1;
    if (token_is_significant(buffers, index)) return index;
  }
  return SIZE_MAX;
}

static size_t next_significant(const w_seed_formatter_buffers *buffers,
                               size_t count, size_t index) {
  size_t next = index + 1;
  while (next < count) {
    if (token_is_significant(buffers, next)) return next;
    next += 1;
  }
  return SIZE_MAX;
}

static bool build_groups(const w_seed_parser *parser,
                         const w_seed_formatter_buffers *buffers,
                         size_t token_count, size_t *group_count_out) {
  if (buffers->groups == NULL || buffers->group_capacity == 0) return false;
  size_t group_count = 0;
  for (size_t index = 0; index < token_count; index += 1) {
    if (!token_is_significant(buffers, index)) continue;
    const w_seed_span span = buffers->tokens[index].span;
    if (token_length(&(fmt_writer){.parser = parser, .buffers = buffers}, index) !=
            1 ||
        (!is_open_char(source_bytes(&(fmt_writer){.parser = parser})[span.start_byte]) &&
         !is_close_char(source_bytes(&(fmt_writer){.parser = parser})[span.start_byte]))) {
      continue;
    }
    const char character = (char)source_bytes(&(fmt_writer){.parser = parser})[span.start_byte];
    if (is_open_char((uint8_t)character)) {
      if (group_count >= buffers->group_capacity) return false;
      buffers->groups[group_count].open_token = index;
      buffers->groups[group_count].close_token = SIZE_MAX;
      buffers->groups[group_count].open_char = character;
      buffers->groups[group_count].close_char = close_for(character);
      buffers->groups[group_count].owner = W_SEED_CST_NONE;
      buffers->groups[group_count].multiline = false;
      group_count += 1;
    } else {
      size_t found = group_count;
      while (found != 0) {
        found -= 1;
        if (buffers->groups[found].close_token == SIZE_MAX &&
            buffers->groups[found].close_char == character) {
          break;
        }
      }
      if (found == group_count) return false;
      buffers->groups[found].close_token = index;
    }
  }
  for (size_t index = 0; index < group_count; index += 1) {
    if (buffers->groups[index].close_token == SIZE_MAX) return false;
  }
  *group_count_out = group_count;
  return true;
}

static size_t find_group_by_open(const w_seed_formatter_buffers *buffers,
                                 size_t group_count, size_t open_token) {
  for (size_t index = 0; index < group_count; index += 1) {
    if (buffers->groups[index].open_token == open_token) return index;
  }
  return SIZE_MAX;
}

static size_t find_group_by_close(const w_seed_formatter_buffers *buffers,
                                  size_t group_count, size_t close_token) {
  for (size_t index = 0; index < group_count; index += 1) {
    if (buffers->groups[index].close_token == close_token) return index;
  }
  return SIZE_MAX;
}

static bool token_inside_group(const w_seed_formatter_buffers *buffers,
                               size_t group_index,
                               size_t token_index) {
  const w_seed_format_group *group = &buffers->groups[group_index];
  return group->open_token < token_index && token_index < group->close_token;
}

static bool direct_child_has_comment(const w_seed_parser *parser,
                                     w_seed_cst_index node_index) {
  const w_seed_cst_node *node = &parser->nodes[node_index];
  for (w_seed_cst_index child = node->first_child; child != W_SEED_CST_NONE;
       child = parser->nodes[child].next_sibling) {
    const w_seed_cst_node *child_node = &parser->nodes[child];
    if ((child_node->flags & W_SEED_CST_FLAG_TRIVIA) != 0 &&
        child_node->raw_span.start_byte < child_node->raw_span.end_byte) {
      const uint8_t *bytes = parser->source->bytes.data;
      if (child_node->raw_span.end_byte - child_node->raw_span.start_byte >= 2 &&
          ((bytes[child_node->raw_span.start_byte] == (uint8_t)'/' &&
            (bytes[child_node->raw_span.start_byte + 1] == (uint8_t)'/' ||
             bytes[child_node->raw_span.start_byte + 1] == (uint8_t)'*')))) {
        return true;
      }
    }
  }
  return false;
}

static size_t direct_statement_count(const w_seed_parser *parser,
                                     w_seed_cst_index owner) {
  size_t count = 0;
  if (owner == W_SEED_CST_NONE) return 0;
  for (w_seed_cst_index child = parser->nodes[owner].first_child;
       child != W_SEED_CST_NONE; child = parser->nodes[child].next_sibling) {
    if (node_is_statement(parser->nodes[child].kind)) count += 1;
  }
  return count;
}

static bool is_binary_chain_token(const fmt_writer *writer, size_t index) {
  return token_text(writer, index, "&&") || token_text(writer, index, "||");
}

static size_t count_binary_chain(const fmt_writer *writer, size_t begin,
                                 size_t end) {
  size_t count = 0;
  for (size_t index = begin; index < end; index += 1) {
    if (is_binary_chain_token(writer, index)) count += 1;
  }
  return count;
}

static bool group_has_token_text(const fmt_writer *writer,
                                 const w_seed_format_group *group,
                                 size_t token_count, const char *text) {
  for (size_t index = group->open_token + 1; index < group->close_token;
       index += 1) {
    if (token_is_significant(writer->buffers, index) &&
        token_text(writer, index, text)) {
      return true;
    }
  }
  (void)token_count;
  return false;
}

static bool group_is_nested(const w_seed_formatter_buffers *buffers,
                            size_t outer, size_t candidate);

static bool group_has_top_level_token_text(const fmt_writer *writer,
                                           size_t group_index,
                                           size_t group_count, const char *text) {
  const w_seed_format_group *group = &writer->buffers->groups[group_index];
  for (size_t index = group->open_token + 1; index < group->close_token;
       index += 1) {
    if (!token_is_significant(writer->buffers, index) ||
        !token_text(writer, index, text)) {
      continue;
    }
    bool nested = false;
    for (size_t candidate = 0; candidate < group_count; candidate += 1) {
      if (candidate != group_index &&
          group_is_nested(writer->buffers, group_index, candidate) &&
          token_inside_group(writer->buffers, candidate, index)) {
        nested = true;
        break;
      }
    }
    if (!nested) return true;
  }
  return false;
}

static bool token_inside_angle(const fmt_writer *writer, size_t index,
                               size_t token_count);

static size_t top_level_comma_count(const fmt_writer *writer,
                                    size_t group_index, size_t group_count,
                                    size_t token_count) {
  const w_seed_format_group *group = &writer->buffers->groups[group_index];
  size_t count = 0;
  for (size_t index = group->open_token + 1; index < group->close_token;
       index += 1) {
    if (!token_is_significant(writer->buffers, index) ||
        !token_text(writer, index, ",")) {
      continue;
    }
    if (token_inside_angle(writer, index, token_count)) {
      continue;
    }
    bool nested = false;
    for (size_t nested_group = 0; nested_group < group_count; nested_group += 1) {
      if (nested_group == group_index) continue;
      if (writer->buffers->groups[nested_group].open_token < index &&
          index < writer->buffers->groups[nested_group].close_token &&
          writer->buffers->groups[nested_group].open_token > group->open_token &&
          writer->buffers->groups[nested_group].close_token < group->close_token) {
        nested = true;
        break;
      }
    }
    if (!nested) count += 1;
  }
  return count;
}

static bool group_has_nested_multiline(const w_seed_formatter_buffers *buffers,
                                       size_t group_index, size_t group_count) {
  const w_seed_format_group *group = &buffers->groups[group_index];
  for (size_t index = 0; index < group_count; index += 1) {
    if (index != group_index && buffers->groups[index].multiline &&
        group->open_token < buffers->groups[index].open_token &&
        buffers->groups[index].close_token < group->close_token) {
      return true;
    }
  }
  return false;
}

static void assign_group_owners(const w_seed_parser *parser,
                                const w_seed_formatter_buffers *buffers,
                                size_t group_count) {
  for (size_t group_index = 0; group_index < group_count; group_index += 1) {
    w_seed_format_group *group = &buffers->groups[group_index];
    if (group->open_char != '{') continue;
    const size_t open_byte = buffers->tokens[group->open_token].span.start_byte;
    const size_t close_byte = buffers->tokens[group->close_token].span.end_byte;
    for (size_t node_index = 0; node_index < parser->node_count; node_index += 1) {
      const w_seed_cst_node *node = &parser->nodes[node_index];
      const bool block_owner = node->kind == W_SEED_CST_BLOCK &&
                               node->raw_span.start_byte == open_byte &&
                               node->raw_span.end_byte >= close_byte;
      const bool struct_owner = node->kind == W_SEED_CST_STRUCT &&
                                node->raw_span.start_byte < open_byte &&
                                node->raw_span.end_byte >= close_byte;
      if (block_owner || struct_owner) {
        group->owner = (w_seed_cst_index)node_index;
        break;
      }
    }
  }
}

static bool punctuation_token(const fmt_writer *writer, size_t index,
                              const char *text);
static bool token_inside_angle(const fmt_writer *writer, size_t index,
                               size_t token_count);

static bool parameter_group_for_function(const fmt_writer *writer,
                                         size_t group_index) {
  const w_seed_format_group *group = &writer->buffers->groups[group_index];
  if (group->open_char != '(' || !group->multiline) return false;
  size_t index = group->open_token;
  while (index != 0) {
    index = previous_significant(writer->buffers, index);
    if (index == SIZE_MAX) break;
    if (punctuation_token(writer, index, "{")) return false;
    if (token_text(writer, index, "fn")) return true;
  }
  return false;
}

static bool prefix_boundary_token(const fmt_writer *writer, size_t index) {
  static const char *const boundaries[] = {
      "fn",       "return", "let",   "var",   "spawn", "if",
      "repeat",   "for",    "break", "continue", "commit", "expect",
      "allocator", "entry", "&&", "||",
  };
  for (size_t candidate = 0;
       candidate < sizeof(boundaries) / sizeof(boundaries[0]); candidate += 1) {
    if (token_text(writer, index, boundaries[candidate])) return true;
  }
  return false;
}

/* Estimate the compact semantic width without consulting source trivia. The
 * estimate is deliberately conservative: a long compact group is rendered
 * multiline even when punctuation would reduce the exact width slightly. */
static size_t compact_group_width(const fmt_writer *writer, size_t group_index,
                                  size_t token_count) {
  const w_seed_format_group *group = &writer->buffers->groups[group_index];
  size_t width = 1;
  bool have_previous = false;
  for (size_t index = group->open_token + 1; index < group->close_token;
       index += 1) {
    if (!token_is_significant(writer->buffers, index)) continue;
    if (have_previous && !checked_add_size(&width, 1)) return SIZE_MAX;
    const size_t length = token_length(writer, index);
    if (!checked_add_size(&width, length)) return SIZE_MAX;
    have_previous = true;
  }
  if (group->close_token < token_count) {
    if (have_previous && !checked_add_size(&width, 1)) return SIZE_MAX;
    const size_t close_length = token_length(writer, group->close_token);
    if (!checked_add_size(&width, close_length)) return SIZE_MAX;
  }
  /* Include the semantic prefix on the current canonical line. This avoids
   * leaving a long function/call head above the preferred column even when
   * the delimiter contents alone are short. */
  size_t prefix = 0;
  size_t previous = group->open_token;
  while (previous != 0) {
    previous = previous_significant(writer->buffers, previous);
    if (previous == SIZE_MAX) break;
    if (token_text(writer, previous, "{") || token_text(writer, previous, "}") ||
        token_text(writer, previous, ";")) {
      break;
    }
    if (prefix != 0) {
      if (!checked_add_size(&prefix, 1)) break;
    }
    const size_t length = token_length(writer, previous);
    if (!checked_add_size(&prefix, length)) break;
    if (prefix_boundary_token(writer, previous)) break;
  }
  if (!checked_add_size(&width, prefix)) return SIZE_MAX;
  return width;
}

static void assign_multiline(const w_seed_parser *parser,
                             const w_seed_formatter_buffers *buffers,
                             size_t token_count, size_t group_count) {
  fmt_writer view = {.parser = parser, .buffers = buffers};
  for (size_t group_index = 0; group_index < group_count; group_index += 1) {
    w_seed_format_group *group = &buffers->groups[group_index];
    const size_t open = group->open_token;
    const size_t close = group->close_token;
    if (group->open_char == '(') {
      const size_t comma_count = top_level_comma_count(&view, group_index,
                                                        group_count, token_count);
      const bool const_mode = group_has_token_text(&view, group, token_count,
                                                    "const");
      const bool contract_mode = group_has_token_text(
          &view, group, token_count, "inout") ||
          group_has_token_text(&view, group, token_count, "take") ||
          const_mode;
      group->multiline = group->multiline || comma_count >= 4 ||
                         (const_mode && comma_count >= 1) ||
                         (contract_mode && comma_count >= 2) ||
                         compact_group_width(&view, group_index, token_count) >
                             FMT_PREFERRED_COLUMNS;
      continue;
    }
    if (group->open_char != '{') continue;
    size_t foreign_count = 0;
    for (size_t index = open + 1; index < close; index += 1) {
      if (buffers->tokens[index].kind == FMT_TOKEN_FOREIGN) {
        foreign_count += 1;
        if (span_contains_newline(&view, buffers->tokens[index].span)) {
          group->multiline = true;
        }
      }
    }
    if (foreign_count != 0) continue;
    if (group_has_token_text(&view, group, token_count, "case")) {
      group->multiline = true;
    }
    if (group->owner != W_SEED_CST_NONE) {
      const size_t statements = direct_statement_count(parser, group->owner);
      if (statements > 1 || direct_child_has_comment(parser, group->owner)) {
        group->multiline = true;
      }
      if (count_binary_chain(&view, open + 1, close) >= 2) {
        group->multiline = true;
      }
      if (group_has_token_text(&view, group, token_count, "lock")) {
        group->multiline = true;
      }
      if (group_has_token_text(&view, group, token_count, "allocator")) {
        group->multiline = true;
      }
      size_t prior_token = open;
      while (prior_token != 0) {
        prior_token = previous_significant(buffers, prior_token);
        if (prior_token == SIZE_MAX) break;
        if (token_text(&view, prior_token, "allocator")) {
          group->multiline = true;
          break;
        }
        if (punctuation_token(&view, prior_token, ";") ||
            punctuation_token(&view, prior_token, "{")) {
          break;
        }
      }
      for (size_t nested = 0; nested < group_count; nested += 1) {
        if (nested != group_index &&
            buffers->groups[nested].open_char == '{' &&
            group->open_token < buffers->groups[nested].open_token &&
            buffers->groups[nested].close_token < group->close_token &&
            group_has_token_text(&view, &buffers->groups[nested], token_count,
                                 "case")) {
          group->multiline = true;
          break;
        }
      }
      if (group_has_nested_multiline(buffers, group_index, group_count)) {
        group->multiline = true;
      }
      /* A multiline declaration parameter list keeps its function body in
         the same vertical layout. */
      for (size_t parameter_group = 0; parameter_group < group_count;
           parameter_group += 1) {
        if (buffers->groups[parameter_group].close_token < open &&
            parameter_group_for_function(&view, parameter_group)) {
          group->multiline = true;
          break;
        }
      }
      const size_t before_block = previous_significant(buffers, open);
      if (before_block != SIZE_MAX && token_text(&view, before_block, "else")) {
        /* Value-if branches share one vertical layout once either branch
           owns multiple statements. */
        for (size_t prior = 0; prior < group_count; prior += 1) {
          if (buffers->groups[prior].open_char == '{' &&
              buffers->groups[prior].close_token < open &&
              buffers->groups[prior].multiline) {
            group->multiline = true;
            break;
          }
        }
      }
      if (statements == 1 && group_has_token_text(&view, group, token_count, "if")) {
        size_t nested_braces = 0;
        for (size_t nested = 0; nested < group_count; nested += 1) {
          if (nested != group_index &&
              buffers->groups[nested].open_char == '{' &&
              group->open_token < buffers->groups[nested].open_token &&
              buffers->groups[nested].close_token < group->close_token) {
            nested_braces += 1;
          }
        }
        if (nested_braces >= 2) group->multiline = true;
      }
    }
  }
}

static void writer_byte(fmt_writer *writer, uint8_t value) {
  (void)checked_add_size(&writer->required, 1);
  if (writer->output != NULL && writer->written < writer->capacity) {
    writer->output[writer->written] = value;
    writer->written += 1;
  }
  writer->last = value;
  if (value == (uint8_t)'\n') {
    writer->line_start = true;
    writer->column = 0;
  } else {
    writer->line_start = false;
    (void)checked_add_size(&writer->column, 1);
  }
}

static void writer_span(fmt_writer *writer, w_seed_span span) {
  for (size_t offset = span.start_byte; offset < span.end_byte; offset += 1) {
    writer_byte(writer, source_bytes(writer)[offset]);
  }
}

static void writer_text(fmt_writer *writer, const char *text) {
  const size_t length = strlen(text);
  for (size_t index = 0; index < length; index += 1) {
    writer_byte(writer, (uint8_t)text[index]);
  }
}

static void writer_indent(fmt_writer *writer, size_t indent) {
  if (!writer->line_start) return;
  for (size_t index = 0; index < indent; index += 1) {
    writer_text(writer, "  ");
  }
}

static void writer_space(fmt_writer *writer) {
  if (!writer->line_start && writer->last != (uint8_t)' ') {
    writer_byte(writer, (uint8_t)' ');
  }
}

static void writer_newline(fmt_writer *writer) {
  if (!writer->line_start) writer_byte(writer, (uint8_t)'\n');
}

static bool punctuation_token(const fmt_writer *writer, size_t index,
                              const char *text) {
  return token_is_significant(writer->buffers, index) &&
         token_text(writer, index, text);
}

static bool word_like(const fmt_writer *writer, size_t index) {
  if (!token_is_significant(writer->buffers, index)) return false;
  if (writer->buffers->tokens[index].kind == FMT_TOKEN_FOREIGN) return true;
  return !punctuation_token(writer, index, "(") &&
         !punctuation_token(writer, index, ")") &&
         !punctuation_token(writer, index, "[") &&
         !punctuation_token(writer, index, "]") &&
         !punctuation_token(writer, index, "{") &&
         !punctuation_token(writer, index, "}") &&
         !punctuation_token(writer, index, ",") &&
         !punctuation_token(writer, index, ";") &&
         !punctuation_token(writer, index, ":") &&
         !punctuation_token(writer, index, "#") &&
         !punctuation_token(writer, index, ".") &&
         !punctuation_token(writer, index, "?.") &&
         !punctuation_token(writer, index, "<") &&
         !punctuation_token(writer, index, ">") &&
         !punctuation_token(writer, index, "=") &&
         !punctuation_token(writer, index, "+") &&
         !punctuation_token(writer, index, "-") &&
         !punctuation_token(writer, index, "*") &&
         !punctuation_token(writer, index, "/") &&
         !punctuation_token(writer, index, "%") &&
         !punctuation_token(writer, index, "&&") &&
         !punctuation_token(writer, index, "||") &&
         !punctuation_token(writer, index, "|>") &&
         !punctuation_token(writer, index, "==") &&
         !punctuation_token(writer, index, "!=") &&
         !punctuation_token(writer, index, "<=") &&
         !punctuation_token(writer, index, ">=") &&
         !punctuation_token(writer, index, "+=") &&
         !punctuation_token(writer, index, "=>");
}

static bool angle_compact(const fmt_writer *writer, size_t index,
                          size_t token_count) {
  if (!punctuation_token(writer, index, "<")) return false;
  const size_t previous = previous_significant(writer->buffers, index);
  // Pipeline contracts are source-visible angle envelopes even when their
  // fields use an object literal. Keep the current spelling compact at the
  // pipeline head; the interior object remains formatted normally.
  if (previous != SIZE_MAX && token_text(writer, previous, "pipeline")) return true;
  const size_t next = next_significant(writer->buffers, token_count, index);
  if (next == SIZE_MAX || punctuation_token(writer, next, "=") ||
      punctuation_token(writer, next, "&&") ||
      punctuation_token(writer, next, "||")) {
    return false;
  }
  if (punctuation_token(writer, next, "[") ||
      (punctuation_token(writer, next, "(") && previous != SIZE_MAX &&
       punctuation_token(writer, previous, ">"))) {
    return true;
  }
  size_t depth = 1;
  for (size_t cursor = next; cursor < token_count; cursor += 1) {
    if (!token_is_significant(writer->buffers, cursor)) continue;
    if (punctuation_token(writer, cursor, "<")) {
      depth += 1;
    } else if (punctuation_token(writer, cursor, ">")) {
      depth -= 1;
      if (depth == 0) return true;
    } else if (punctuation_token(writer, cursor, ";") ||
               punctuation_token(writer, cursor, "{") ||
               punctuation_token(writer, cursor, "}")) {
      return false;
    } else if (punctuation_token(writer, cursor, "<=") ||
               punctuation_token(writer, cursor, ">=") ||
               punctuation_token(writer, cursor, "==") ||
               punctuation_token(writer, cursor, "&&") ||
               punctuation_token(writer, cursor, "||") ||
               punctuation_token(writer, cursor, "+") ||
               punctuation_token(writer, cursor, "-") ||
               punctuation_token(writer, cursor, "*")) {
      return false;
    }
  }
  return false;
}

static bool angle_close_compact(const fmt_writer *writer, size_t index,
                                size_t token_count) {
  if (!punctuation_token(writer, index, ">")) return false;
  size_t depth = 1;
  size_t cursor = index;
  while (cursor != 0) {
    cursor = previous_significant(writer->buffers, cursor);
    if (cursor == SIZE_MAX) break;
    if (punctuation_token(writer, cursor, ">")) {
      depth += 1;
    } else if (punctuation_token(writer, cursor, "<")) {
      depth -= 1;
      if (depth == 0) return angle_compact(writer, cursor, token_count);
    }
  }
  return false;
}

static bool token_inside_angle(const fmt_writer *writer, size_t index,
                               size_t token_count) {
  size_t depth = 0;
  size_t cursor = index;
  while (cursor != 0) {
    cursor = previous_significant(writer->buffers, cursor);
    if (cursor == SIZE_MAX) break;
    if (punctuation_token(writer, cursor, ">")) {
      depth += 1;
    } else if (punctuation_token(writer, cursor, "<")) {
      if (!angle_compact(writer, cursor, token_count)) return false;
      if (depth == 0) return true;
      depth -= 1;
    } else if (punctuation_token(writer, cursor, "{") ||
               punctuation_token(writer, cursor, ";")) {
      return false;
    }
  }
  return false;
}

static bool binary_operator(const fmt_writer *writer, size_t index) {
  return punctuation_token(writer, index, "=") ||
         punctuation_token(writer, index, "+=") ||
         punctuation_token(writer, index, "+") ||
         punctuation_token(writer, index, "-") ||
         punctuation_token(writer, index, "*") ||
         punctuation_token(writer, index, "/") ||
         punctuation_token(writer, index, "%") ||
         punctuation_token(writer, index, "==") ||
         punctuation_token(writer, index, "!=") ||
         punctuation_token(writer, index, "<") ||
         punctuation_token(writer, index, ">") ||
         punctuation_token(writer, index, "<=") ||
         punctuation_token(writer, index, ">=") ||
         punctuation_token(writer, index, "&&") ||
         punctuation_token(writer, index, "||") ||
         punctuation_token(writer, index, "|>") ||
         punctuation_token(writer, index, "=>");
}

static bool should_keep_semicolon(const fmt_writer *writer, size_t index,
                                  size_t token_count, size_t group_count) {
  for (size_t group = 0; group < group_count; group += 1) {
    if (writer->buffers->groups[group].open_char == '[' &&
        token_inside_group(writer->buffers, group, index)) {
      return true;
    }
  }
  if (writer->active_multiline_blocks == 0) return false;
  const size_t next = next_significant(writer->buffers, token_count, index);
  return next != SIZE_MAX &&
         (punctuation_token(writer, next, ".") ||
          punctuation_token(writer, next, "(") ||
          punctuation_token(writer, next, "["));
}

static bool needs_space(const fmt_writer *writer, size_t index,
                        size_t token_count) {
  if (writer->line_start || !writer->have_previous) return false;
  const size_t previous = writer->previous_significant;
  if (punctuation_token(writer, index, ",") ||
      punctuation_token(writer, index, ";") ||
      punctuation_token(writer, index, ":") ||
      punctuation_token(writer, index, "#") ||
      punctuation_token(writer, index, ")") ||
       punctuation_token(writer, index, "]") ||
       punctuation_token(writer, index, ".") ||
      punctuation_token(writer, index, "?.")) {
    if (punctuation_token(writer, index, ".") &&
        (token_text(writer, previous, "return") ||
         token_text(writer, previous, "case") ||
         token_text(writer, previous, ":") ||
         token_text(writer, previous, ",") ||
         token_text(writer, previous, "allocator"))) {
      return true;
    }
    return false;
  }
  if ((writer->buffers->tokens[previous].flags & FMT_TOKEN_FLAG_LITERAL) != 0 &&
      (writer->buffers->tokens[index].flags & FMT_TOKEN_FLAG_LITERAL) != 0 &&
      writer->buffers->tokens[previous].span.end_byte ==
          writer->buffers->tokens[index].span.start_byte) {
    return false;
  }
  if (punctuation_token(writer, previous, "(") ||
      punctuation_token(writer, previous, "[") ||
      punctuation_token(writer, previous, ".") ||
      punctuation_token(writer, previous, "?.") ||
      punctuation_token(writer, previous, "#")) {
    return false;
  }
  if (punctuation_token(writer, previous, ",") ||
      punctuation_token(writer, previous, ":") ||
      punctuation_token(writer, previous, ";")) {
    return true;
  }
  if (punctuation_token(writer, index, "(")) {
    return punctuation_token(writer, previous, ">") ||
           punctuation_token(writer, previous, "=") ||
           token_text(writer, previous, "await") ||
           token_text(writer, previous, "return");
  }
  if (punctuation_token(writer, index, "[")) {
    return token_text(writer, previous, "return") ||
           token_text(writer, previous, "=");
  }
  if (punctuation_token(writer, index, "<")) {
    if (token_text(writer, previous, "return") ||
        token_text(writer, previous, "=")) return true;
    return !angle_compact(writer, index, token_count);
  }
  if (punctuation_token(writer, previous, "<")) {
    return !angle_compact(writer, previous, token_count);
  }
  if (punctuation_token(writer, index, ">")) {
    return !angle_close_compact(writer, index, token_count);
  }
  if (punctuation_token(writer, previous, ">")) return true;
  if (punctuation_token(writer, index, "{")) return true;
  if (punctuation_token(writer, previous, "}")) return true;
  if (binary_operator(writer, index) || binary_operator(writer, previous)) {
    return true;
  }
  return word_like(writer, previous) && word_like(writer, index);
}

static void emit_significant(fmt_writer *writer, size_t index, size_t indent,
                             size_t token_count, bool allow_semicolon,
                             size_t group_count) {
  writer_indent(writer, indent);
  if (punctuation_token(writer, index, ";") && !allow_semicolon &&
      !should_keep_semicolon(writer, index, token_count, group_count)) {
    return;
  }
  if (needs_space(writer, index, token_count)) writer_space(writer);
  writer_span(writer, writer->buffers->tokens[index].span);
  writer->previous_significant = index;
  writer->have_previous = true;
}

static void emit_comment(fmt_writer *writer, size_t index, size_t indent) {
  writer_indent(writer, indent);
  if (!writer->line_start && writer->last != (uint8_t)' ') writer_space(writer);
  writer_span(writer, writer->buffers->tokens[index].span);
  if (span_starts(writer, writer->buffers->tokens[index].span, "//")) {
    writer_newline(writer);
  } else if (span_contains_newline(writer, writer->buffers->tokens[index].span)) {
    writer_newline(writer);
  } else {
    writer_space(writer);
  }
}

static bool group_is_nested(const w_seed_formatter_buffers *buffers,
                            size_t outer,
                            size_t candidate) {
  if (outer == candidate) return false;
  return buffers->groups[outer].open_token < buffers->groups[candidate].open_token &&
         buffers->groups[candidate].close_token < buffers->groups[outer].close_token;
}

static void render_range(fmt_writer *writer, size_t begin, size_t end,
                         size_t indent, size_t token_count, size_t group_count);

static void render_list_group(fmt_writer *writer, size_t group_index,
                              size_t indent, size_t token_count,
                              size_t group_count) {
  const w_seed_format_group *group = &writer->buffers->groups[group_index];
  const size_t open = group->open_token;
  const size_t close = group->close_token;
  emit_significant(writer, open, indent, token_count, false, group_count);
  size_t segment_start = open + 1;
  bool had_item = false;
  for (size_t index = open + 1; index < close; index += 1) {
    if (!token_is_significant(writer->buffers, index) ||
        !punctuation_token(writer, index, ",")) {
      continue;
    }
    if (token_inside_angle(writer, index, token_count)) continue;
    bool nested = false;
    for (size_t nested_group = 0; nested_group < group_count; nested_group += 1) {
      if (group_is_nested(writer->buffers, group_index,
                          nested_group) &&
          token_inside_group(writer->buffers, nested_group, index)) {
        nested = true;
        break;
      }
    }
    if (nested) continue;
    writer_newline(writer);
    writer_indent(writer, indent + 1);
    render_range(writer, segment_start, index, indent + 1, token_count,
                 group_count);
    emit_significant(writer, index, indent + 1, token_count, false, group_count);
    had_item = true;
    segment_start = index + 1;
  }
  bool trailing_content = false;
  for (size_t index = segment_start; index < close; index += 1) {
    if (writer->buffers->tokens[index].kind != FMT_TOKEN_TRIVIA) {
      trailing_content = true;
      break;
    }
  }
  if (segment_start < close && trailing_content) {
    writer_newline(writer);
    writer_indent(writer, indent + 1);
    render_range(writer, segment_start, close, indent + 1, token_count,
                 group_count);
    /* G5 list layout always carries the trailing separator. */
    const size_t previous = previous_significant(writer->buffers, close);
    if (previous != SIZE_MAX && !punctuation_token(writer, previous, ",")) {
      writer_text(writer, ",");
    }
    had_item = true;
  }
  if (had_item) writer_newline(writer);
  writer_indent(writer, indent);
  emit_significant(writer, close, indent, token_count, false, group_count);
}

static void render_foreign_group(fmt_writer *writer, size_t group_index,
                                 size_t indent, size_t token_count,
                                 size_t group_count) {
  const w_seed_format_group *group = &writer->buffers->groups[group_index];
  emit_significant(writer, group->open_token, indent, token_count, false,
                   group_count);
  for (size_t index = group->open_token + 1; index < group->close_token;
       index += 1) {
    if (writer->buffers->tokens[index].kind == FMT_TOKEN_FOREIGN) {
      writer_span(writer, writer->buffers->tokens[index].span);
      writer->previous_significant = index;
      writer->have_previous = true;
    }
  }
  if (!writer->line_start && writer->last != (uint8_t)' ') writer_space(writer);
  emit_significant(writer, group->close_token, indent, token_count, false,
                   group_count);
}

static void render_switch_body(fmt_writer *writer, size_t group_index,
                               size_t indent, size_t token_count,
                               size_t group_count) {
  const w_seed_format_group *group = &writer->buffers->groups[group_index];
  emit_significant(writer, group->open_token, indent, token_count, false,
                   group_count);
  size_t segment_start = group->open_token + 1;
  bool found_case = false;
  for (size_t index = segment_start; index < group->close_token; index += 1) {
    if (!token_is_significant(writer->buffers, index) ||
        !token_text(writer, index, "case")) {
      continue;
    }
    bool nested = false;
    for (size_t nested_group = 0; nested_group < group_count; nested_group += 1) {
      if (group_is_nested(writer->buffers, group_index,
                          nested_group) &&
          token_inside_group(writer->buffers, nested_group, index)) {
        nested = true;
        break;
      }
    }
    if (nested) continue;
    if (found_case) {
      writer_newline(writer);
      writer_indent(writer, indent + 1);
      render_range(writer, segment_start, index, indent + 1, token_count,
                   group_count);
    }
    found_case = true;
    segment_start = index;
  }
  if (found_case) {
    writer_newline(writer);
    writer_indent(writer, indent + 1);
    render_range(writer, segment_start, group->close_token, indent + 1,
                 token_count, group_count);
  } else {
    render_range(writer, group->open_token + 1, group->close_token, indent + 1,
                 token_count, group_count);
  }
  writer_newline(writer);
  writer_indent(writer, indent);
  emit_significant(writer, group->close_token, indent, token_count, false,
                   group_count);
}

static void render_block_group(fmt_writer *writer, size_t group_index,
                               size_t indent, size_t token_count,
                               size_t group_count) {
  const w_seed_format_group *group = &writer->buffers->groups[group_index];
  if (group->open_char != '{') {
    if (group->multiline) {
      render_list_group(writer, group_index, indent, token_count, group_count);
    } else {
      emit_significant(writer, group->open_token, indent, token_count, false,
                       group_count);
      render_range(writer, group->open_token + 1, group->close_token, indent,
                   token_count, group_count);
      emit_significant(writer, group->close_token, indent, token_count, false,
                       group_count);
    }
    return;
  }
  bool foreign = false;
  for (size_t index = group->open_token + 1; index < group->close_token;
       index += 1) {
    if (writer->buffers->tokens[index].kind == FMT_TOKEN_FOREIGN) {
      foreign = true;
      break;
    }
  }
  if (foreign) {
    render_foreign_group(writer, group_index, indent, token_count, group_count);
    return;
  }
  if (!group->multiline) {
    emit_significant(writer, group->open_token, indent, token_count, false,
                     group_count);
    if (group->open_token + 1 != group->close_token) {
      writer_space(writer);
      render_range(writer, group->open_token + 1, group->close_token, indent,
                   token_count, group_count);
      writer_space(writer);
    }
    emit_significant(writer, group->close_token, indent, token_count, true,
                     group_count);
    return;
  }

  if (group_has_top_level_token_text(writer, group_index, group_count, "case")) {
    render_switch_body(writer, group_index, indent, token_count, group_count);
    return;
  }

  emit_significant(writer, group->open_token, indent, token_count, false,
                   group_count);
  writer_newline(writer);
  writer->active_multiline_blocks += 1;
  bool rendered_statement = false;
  size_t cursor = group->open_token + 1;
  if (group->owner != W_SEED_CST_NONE) {
    const w_seed_cst_node *owner = &writer->parser->nodes[group->owner];
    for (w_seed_cst_index child = owner->first_child; child != W_SEED_CST_NONE;
         child = writer->parser->nodes[child].next_sibling) {
      const w_seed_cst_node *child_node = &writer->parser->nodes[child];
      const bool render_child = owner->kind == W_SEED_CST_STRUCT
                                    ? child_node->kind == W_SEED_CST_FIELD
                                    : node_is_statement(child_node->kind);
      if (!render_child) continue;
      size_t child_start = cursor;
      while (child_start < group->close_token &&
             writer->buffers->tokens[child_start].span.start_byte <
                 child_node->raw_span.start_byte) {
        child_start += 1;
      }
      if (child_start > cursor) {
        render_range(writer, cursor, child_start, indent + 1, token_count,
                     group_count);
      }
      writer_newline(writer);
      writer_indent(writer, indent + 1);
      size_t child_end = child_start;
      while (child_end < group->close_token &&
             writer->buffers->tokens[child_end].span.end_byte <=
                 child_node->raw_span.end_byte) {
        child_end += 1;
      }
      render_range(writer, child_start, child_end, indent + 1, token_count,
                   group_count);
      writer_newline(writer);
      cursor = child_end;
      rendered_statement = true;
    }
  }
  if (!rendered_statement) {
    if (group_has_top_level_token_text(writer, group_index, group_count, "case")) {
      render_switch_body(writer, group_index, indent, token_count, group_count);
      writer->active_multiline_blocks -= 1;
      return;
    }
    render_range(writer, cursor, group->close_token, indent + 1, token_count,
                 group_count);
  } else if (cursor < group->close_token) {
    render_range(writer, cursor, group->close_token, indent + 1, token_count,
                 group_count);
  }
  writer->active_multiline_blocks -= 1;
  writer_newline(writer);
  writer_indent(writer, indent);
  emit_significant(writer, group->close_token, indent, token_count, false,
                   group_count);
}

static void render_range(fmt_writer *writer, size_t begin, size_t end,
                         size_t indent, size_t token_count, size_t group_count) {
  size_t index = begin;
  while (index < end) {
    const w_seed_format_token *token = &writer->buffers->tokens[index];
    if (token->kind == FMT_TOKEN_TRIVIA) {
      index += 1;
      continue;
    }
    if (token->kind == FMT_TOKEN_COMMENT) {
      emit_comment(writer, index, indent);
      index += 1;
      continue;
    }
    if (token->kind == FMT_TOKEN_FOREIGN) {
      emit_significant(writer, index, indent, token_count, false, group_count);
      index += 1;
      continue;
    }
    const size_t group = find_group_by_open(writer->buffers, group_count, index);
    if (group != SIZE_MAX) {
      render_block_group(writer, group, indent, token_count, group_count);
      index = writer->buffers->groups[group].close_token + 1;
      continue;
    }
    if (find_group_by_close(writer->buffers, group_count, index) != SIZE_MAX) {
      index += 1;
      continue;
    }
    if (is_binary_chain_token(writer, index) &&
        count_binary_chain(writer, begin, end) >= 2) {
      writer_newline(writer);
      writer_indent(writer, indent + 1);
    }
    emit_significant(writer, index, indent, token_count, false, group_count);
    index += 1;
  }
}

static void render_document_bounded(fmt_writer *writer, size_t token_count,
                                    size_t group_count) {
  const w_seed_cst_node *root = &writer->parser->nodes[writer->parser->root];
  size_t cursor = 0;
  bool rendered_owner = false;
  w_seed_cst_kind previous_owner_kind = W_SEED_CST_NONE;
  for (w_seed_cst_index child = root->first_child; child != W_SEED_CST_NONE;
       child = writer->parser->nodes[child].next_sibling) {
    const w_seed_cst_node *node = &writer->parser->nodes[child];
    if ((node->flags & W_SEED_CST_FLAG_RAW_LEAF) != 0) continue;
    size_t start = cursor;
    while (start < token_count &&
           writer->buffers->tokens[start].span.start_byte < node->raw_span.start_byte) {
      start += 1;
    }
    bool preamble_comment = false;
    for (size_t trivia = cursor; trivia < start; trivia += 1) {
      if (writer->buffers->tokens[trivia].kind == FMT_TOKEN_COMMENT) {
        preamble_comment = true;
        break;
      }
    }
    if (start > cursor) {
      render_range(writer, cursor, start, 0, token_count, group_count);
      if (!rendered_owner && preamble_comment) writer_byte(writer, (uint8_t)'\n');
    }
    if (rendered_owner) {
      writer_newline(writer);
      if (!(previous_owner_kind == W_SEED_CST_IMPORT &&
            node->kind == W_SEED_CST_IMPORT)) {
        writer_byte(writer, (uint8_t)'\n');
      }
    }
    size_t end = start;
    while (end < token_count &&
           writer->buffers->tokens[end].span.end_byte <= node->raw_span.end_byte) {
      end += 1;
    }
    render_range(writer, start, end, 0, token_count, group_count);
    cursor = end;
    rendered_owner = true;
    previous_owner_kind = node->kind;
  }
  if (cursor < token_count) render_range(writer, cursor, token_count, 0,
                                         token_count, group_count);
  writer_newline(writer);
}

w_seed_format_status w_seed_formatter_format(
    const w_seed_parser *parser, const w_seed_formatter_buffers *buffers,
    uint8_t *output, size_t output_capacity, w_seed_format_result *result) {
  if (result != NULL) {
    (void)memset(result, 0, sizeof(*result));
    result->status = W_SEED_FORMAT_INVALID;
  }
  if (result == NULL || buffers == NULL || !parser_is_clean(parser)) {
    if (result != NULL) result->status = W_SEED_FORMAT_REJECTED;
    return W_SEED_FORMAT_REJECTED;
  }
  size_t token_count = 0;
  size_t group_count = 0;
  if (!build_tokens(parser, buffers, &token_count) ||
      !build_groups(parser, buffers, token_count, &group_count)) {
    result->status = W_SEED_FORMAT_CAPACITY;
    result->token_count = token_count;
    result->group_count = group_count;
    return result->status;
  }
  assign_group_owners(parser, buffers, group_count);
  /* Nested value blocks propagate their layout requirement outward. A small
     fixed point is sufficient for the bounded seed without heap state. */
  for (size_t pass = 0; pass < 4; pass += 1) {
    assign_multiline(parser, buffers, token_count, group_count);
  }

  fmt_writer measure = {
      .parser = parser,
      .buffers = buffers,
      .output = NULL,
      .capacity = 0,
      .required = 0,
      .written = 0,
      .line_start = true,
      .last = (uint8_t)'\n',
      .column = 0,
      .previous_significant = SIZE_MAX,
      .have_previous = false,
  };
  render_document_bounded(&measure, token_count, group_count);
  result->required_bytes = measure.required;
  result->token_count = token_count;
  result->group_count = group_count;
  if (output == NULL || output_capacity < measure.required) {
    result->status = W_SEED_FORMAT_CAPACITY;
    return result->status;
  }
  fmt_writer write = measure;
  write.output = output;
  write.capacity = output_capacity;
  write.required = 0;
  write.written = 0;
  write.line_start = true;
  write.last = (uint8_t)'\n';
  write.column = 0;
  write.previous_significant = SIZE_MAX;
  write.have_previous = false;
  render_document_bounded(&write, token_count, group_count);
  result->status = W_SEED_FORMAT_OK;
  result->written_bytes = write.written;
  result->required_bytes = write.required;
  return result->status;
}
