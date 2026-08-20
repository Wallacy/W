#include "w_seed_parser.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "parser check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                       \
      return false;                                                             \
    }                                                                           \
  } while (0)

typedef struct {
  w_seed_source source;
  w_seed_lexer_frame lexer_frames[64];
  w_seed_parse_token tokens[32];
  w_seed_cst_node nodes[512];
  w_seed_parse_frame frames[128];
  w_seed_parse_issue issues[32];
  w_seed_parser parser;
  w_seed_parse_result result;
} fixture;

static bool fixture_init(fixture *fixture_value, const char *text,
                         size_t node_capacity, size_t issue_capacity) {
  const w_seed_byte_view bytes = {(const uint8_t *)text, strlen(text)};
  w_seed_source_error source_error;
  CHECK(w_seed_source_init(bytes, &fixture_value->source, &source_error));
  w_seed_lex_error lex_error;
  const w_seed_span bounds = {0, bytes.length};
  CHECK(w_seed_parser_init(
      &fixture_value->source, bounds, fixture_value->lexer_frames,
      sizeof(fixture_value->lexer_frames) /
          sizeof(fixture_value->lexer_frames[0]),
      fixture_value->tokens, sizeof(fixture_value->tokens) /
                                 sizeof(fixture_value->tokens[0]),
      fixture_value->nodes, node_capacity, fixture_value->frames,
      sizeof(fixture_value->frames) / sizeof(fixture_value->frames[0]),
      fixture_value->issues, issue_capacity, &fixture_value->parser,
      &lex_error));
  CHECK(w_seed_parser_parse(&fixture_value->parser, &fixture_value->result));
  return true;
}

static bool fixture_init_range(fixture *fixture_value, const char *text,
                               w_seed_span bounds) {
  const w_seed_byte_view bytes = {(const uint8_t *)text, strlen(text)};
  w_seed_source_error source_error;
  CHECK(w_seed_source_init(bytes, &fixture_value->source, &source_error));
  w_seed_lex_error lex_error;
  CHECK(w_seed_parser_init(
      &fixture_value->source, bounds, fixture_value->lexer_frames,
      sizeof(fixture_value->lexer_frames) /
          sizeof(fixture_value->lexer_frames[0]),
      fixture_value->tokens, sizeof(fixture_value->tokens) /
                                 sizeof(fixture_value->tokens[0]),
      fixture_value->nodes, sizeof(fixture_value->nodes) /
                               sizeof(fixture_value->nodes[0]),
      fixture_value->frames, sizeof(fixture_value->frames) /
                                  sizeof(fixture_value->frames[0]),
      fixture_value->issues, sizeof(fixture_value->issues) /
                                 sizeof(fixture_value->issues[0]),
      &fixture_value->parser, &lex_error));
  CHECK(w_seed_parser_parse(&fixture_value->parser, &fixture_value->result));
  return true;
}

static bool check_leaf_partition(const fixture *fixture_value) {
  size_t previous_end = 0;
  size_t leaves = 0;
  for (size_t index = 0; index < fixture_value->result.node_count; index += 1) {
    const w_seed_cst_node *node = &fixture_value->nodes[index];
    if ((node->flags & W_SEED_CST_FLAG_RAW_LEAF) == 0) continue;
    CHECK(node->raw_span.start_byte == previous_end);
    CHECK(node->raw_span.end_byte >= node->raw_span.start_byte);
    previous_end = node->raw_span.end_byte;
    leaves += 1;
  }
  CHECK(previous_end == fixture_value->source.bytes.length);
  CHECK(leaves == fixture_value->result.leaf_count);
  return true;
}

static bool check_leaf_partition_range(const fixture *fixture_value,
                                       w_seed_span bounds) {
  size_t previous_end = bounds.start_byte;
  size_t leaves = 0;
  for (size_t index = 0; index < fixture_value->result.node_count; index += 1) {
    const w_seed_cst_node *node = &fixture_value->nodes[index];
    if ((node->flags & W_SEED_CST_FLAG_RAW_LEAF) == 0) continue;
    CHECK(node->raw_span.start_byte == previous_end);
    CHECK(node->raw_span.end_byte >= node->raw_span.start_byte);
    CHECK(node->raw_span.end_byte <= bounds.end_byte);
    previous_end = node->raw_span.end_byte;
    leaves += 1;
  }
  CHECK(previous_end == bounds.end_byte);
  CHECK(leaves == fixture_value->result.leaf_count);
  return true;
}

static bool check_tree_links(const fixture *fixture_value) {
  for (size_t index = 0; index < fixture_value->result.node_count; index += 1) {
    const w_seed_cst_node *parent = &fixture_value->nodes[index];
    size_t guard = 0;
    size_t previous_end = parent->raw_span.start_byte;
    w_seed_cst_index child = parent->first_child;
    while (child != W_SEED_CST_NONE) {
      CHECK(child < fixture_value->result.node_count);
      const w_seed_cst_node *child_node = &fixture_value->nodes[child];
      CHECK(child_node->raw_span.start_byte >= parent->raw_span.start_byte);
      CHECK(child_node->raw_span.end_byte >= child_node->raw_span.start_byte);
      CHECK(child_node->raw_span.end_byte <= parent->raw_span.end_byte);
      CHECK(child_node->raw_span.start_byte >= previous_end);
      previous_end = child_node->raw_span.end_byte;
      child = child_node->next_sibling;
      guard += 1;
      CHECK(guard <= fixture_value->result.node_count);
    }
  }
  return true;
}

static bool has_issue(const fixture *fixture_value,
                      w_seed_parse_issue_kind kind) {
  for (size_t index = 0; index < fixture_value->result.issue_count; index += 1) {
    if (fixture_value->issues[index].kind == kind) return true;
  }
  return false;
}

static bool has_direct_expression_span(const fixture *fixture_value,
                                       size_t outer_start, size_t outer_end,
                                       size_t nested_start,
                                       size_t nested_end) {
  for (size_t index = 0; index < fixture_value->result.node_count; index += 1) {
    const w_seed_cst_node *node = &fixture_value->nodes[index];
    if (node->kind != W_SEED_CST_EXPRESSION ||
        node->raw_span.start_byte != outer_start ||
        node->raw_span.end_byte != outer_end)
      continue;
    w_seed_cst_index child = node->first_child;
    while (child != W_SEED_CST_NONE) {
      const w_seed_cst_node *child_node = &fixture_value->nodes[child];
      if (child_node->kind == W_SEED_CST_EXPRESSION &&
          child_node->raw_span.start_byte == nested_start &&
          child_node->raw_span.end_byte == nested_end) {
        return true;
      }
      child = child_node->next_sibling;
    }
  }
  return false;
}

static bool test_pratt_shape(const char *text, const char *outer_text,
                             const char *nested_text,
                             const char *forbidden_nested_text) {
  fixture value;
  CHECK(fixture_init(&value, text,
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  const char *outer = strstr(text, outer_text);
  CHECK(outer != NULL);
  const char *nested = strstr(outer, nested_text);
  CHECK(nested != NULL);
  const size_t outer_start = (size_t)(outer - text);
  const size_t outer_end = outer_start + strlen(outer_text);
  const size_t nested_start = (size_t)(nested - text);
  const size_t nested_end = nested_start + strlen(nested_text);
  CHECK(has_direct_expression_span(&value, outer_start, outer_end,
                                   nested_start, nested_end));
  if (forbidden_nested_text != NULL) {
    const char *forbidden = strstr(outer, forbidden_nested_text);
    CHECK(forbidden != NULL);
    const size_t forbidden_start = (size_t)(forbidden - text);
    const size_t forbidden_end = forbidden_start + strlen(forbidden_nested_text);
    CHECK(!has_direct_expression_span(&value, outer_start, outer_end,
                                      forbidden_start, forbidden_end));
  }
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));
  return true;
}

static bool test_positive_core(void) {
  fixture value;
  CHECK(fixture_init(&value,
                     "module m\nfn f(x:Thing):(){let y=x;return y}\nentry(f)\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));
  return true;
}

static bool test_nested_close_and_shift(void) {
  fixture value;
  CHECK(fixture_init(
      &value,
      "fn f(x:Array<Array<u8>>):Array<Array<u8>>{return flags >> 2}\n",
      sizeof(value.nodes) / sizeof(value.nodes[0]),
      sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(check_leaf_partition(&value));
  return true;
}

static bool test_pratt_nesting(void) {
  CHECK(test_pratt_shape("fn f(){a+b*c}\n", "a+b*c", "b*c", NULL));
  CHECK(test_pratt_shape("fn f(){a-b-c}\n", "a-b-c", "c", "b-c"));
  CHECK(test_pratt_shape("fn f(){a**b**c}\n", "a**b**c", "b**c", NULL));
  CHECK(test_pratt_shape("fn f(){a=b=c}\n", "a=b=c", "b=c", NULL));
  return true;
}

static bool test_pratt_operator_table(void) {
  static const char *const operators[] = {
      "=",   "+=",  "-=",  "*=",  "/=",  "%=",  "**=", "<<=",
      ">>=", "&=",  "^=",  "|=",  "??",  "||",  "&&",  "|",
      "^",   "&",   "==",  "!=",  "<",   "<=",  ">",   ">=",
      "...", "..<", ">..", ">..<", "<<",  ">>",  "+",   "-",
      "*",   "/",   "%",   "@",   "**",  "in",   "is",
  };
  for (size_t index = 0;
       index < sizeof(operators) / sizeof(operators[0]); index += 1) {
    char text[96];
    const int written =
        snprintf(text, sizeof(text), "fn f(){a %s b}\n", operators[index]);
    CHECK(written > 0);
    CHECK((size_t)written < sizeof(text));
    fixture value;
    CHECK(fixture_init(&value, text,
                       sizeof(value.nodes) / sizeof(value.nodes[0]),
                       sizeof(value.issues) / sizeof(value.issues[0])));
    CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
    CHECK(check_leaf_partition(&value));
    CHECK(check_tree_links(&value));
  }
  return true;
}

static bool test_subspan_bounds(void) {
  fixture value;
  const w_seed_span bounds = {2, 10};
  CHECK(fixture_init_range(&value, "xxfn f(){}yy", bounds));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(value.nodes[value.result.root].raw_span.start_byte == bounds.start_byte);
  CHECK(value.nodes[value.result.root].raw_span.end_byte == bounds.end_byte);
  CHECK(value.result.consumed_byte == bounds.end_byte);
  CHECK(check_leaf_partition_range(&value, bounds));
  return true;
}

static bool test_adjacency_and_boundaries(void) {
  fixture value;
  CHECK(fixture_init(&value, "fn f(){return try? load()}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(fixture_init(&value, "fn f(){value?.open?}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(fixture_init(&value, "fn f(){left\n  (right)}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(fixture_init(&value, "fn f(){a();(b)c();[d,e]}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(check_leaf_partition(&value));
  return true;
}

static bool test_recovery_codes(void) {
  fixture value;
  CHECK(fixture_init(&value, "fn f(x:Array /* note */ <u8>){return x}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_RECOVERED);
  CHECK(has_issue(&value, W_SEED_PARSE_ISSUE_SPACED_HEAD));
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));

  CHECK(fixture_init(&value, "fn f(){return 1\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_RECOVERED);
  CHECK(has_issue(&value, W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE));
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));

  CHECK(fixture_init(&value, "fn f(){else}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_RECOVERED);
  CHECK(has_issue(&value, W_SEED_PARSE_ISSUE_NO_CONTINUATION_OWNER));
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));

  CHECK(fixture_init(&value, "fn f():Stage{return if ready{.ok}}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_RECOVERED);
  CHECK(has_issue(&value, W_SEED_PARSE_ISSUE_VALUE_IF_MISSING_ELSE));
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));
  return true;
}

static bool test_fail_closed(void) {
  fixture value;
  CHECK(fixture_init(&value, "module m\npackage {name: \"x\"}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_FATAL);
  CHECK(has_issue(&value, W_SEED_PARSE_ISSUE_MIXED_ROOT));
  CHECK(value.result.issue_count == 1);
  CHECK(value.issues[0].kind == W_SEED_PARSE_ISSUE_MIXED_ROOT);
  CHECK(check_leaf_partition(&value));

  CHECK(fixture_init(&value, "package {name: \"x\"}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_FATAL);
  CHECK(value.result.issue_count == 1);
  CHECK(value.issues[0].kind == W_SEED_PARSE_ISSUE_UNSUPPORTED_ROOT);
  CHECK(check_leaf_partition(&value));

  CHECK(fixture_init(&value, "fn f(){foreign c { host body }}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_FATAL);
  CHECK(has_issue(&value, W_SEED_PARSE_ISSUE_FOREIGN_UNSUPPORTED));
  CHECK(value.result.issue_count == 1);
  CHECK(value.issues[0].kind == W_SEED_PARSE_ISSUE_FOREIGN_UNSUPPORTED);
  CHECK(check_leaf_partition(&value));
  return true;
}

static bool test_capacity(void) {
  fixture value;
  CHECK(fixture_init(&value, "fn f(){return 1}\n", 4,
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_FATAL);
  CHECK(has_issue(&value, W_SEED_PARSE_ISSUE_CAPACITY));
  return true;
}

static bool test_init_validation(void) {
  fixture value;
  const w_seed_span bounds = {0, 0};
  w_seed_lex_error lex_error;
  CHECK(!w_seed_parser_init(NULL, bounds, NULL, 0, NULL, 0, NULL, 0, NULL,
                            0, NULL, 0, &value.parser, &lex_error));
  return true;
}

static bool test_parse_twice(void) {
  fixture value;
  CHECK(fixture_init(&value, "fn f(){return 1}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  const w_seed_parse_result first = value.result;
  w_seed_parse_result second = {W_SEED_PARSE_FATAL, W_SEED_CST_NONE, 17, 19,
                                23, 29};
  CHECK(!w_seed_parser_parse(&value.parser, &second));
  CHECK(memcmp(&value.result, &first, sizeof(first)) == 0);
  CHECK(second.status == W_SEED_PARSE_FATAL);
  CHECK(second.root == W_SEED_CST_NONE);
  CHECK(second.node_count == 17);
  CHECK(second.leaf_count == 19);
  CHECK(second.issue_count == 23);
  CHECK(second.consumed_byte == 29);
  return true;
}

int main(void) {
  CHECK(test_positive_core());
  CHECK(test_nested_close_and_shift());
  CHECK(test_pratt_nesting());
  CHECK(test_pratt_operator_table());
  CHECK(test_subspan_bounds());
  CHECK(test_adjacency_and_boundaries());
  CHECK(test_recovery_codes());
  CHECK(test_fail_closed());
  CHECK(test_capacity());
  CHECK(test_init_validation());
  CHECK(test_parse_twice());
  (void)puts("Seed C parser: caller-owned CST, recovery and P0a hand cases passed");
  return 0;
}
