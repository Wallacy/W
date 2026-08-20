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

static size_t count_direct_kind(const fixture *fixture_value,
                                w_seed_cst_index parent_index,
                                w_seed_cst_kind kind) {
  size_t count = 0;
  if (parent_index >= fixture_value->result.node_count) return 0;
  w_seed_cst_index child = fixture_value->nodes[parent_index].first_child;
  size_t guard = 0;
  while (child != W_SEED_CST_NONE && guard <= fixture_value->result.node_count) {
    if (fixture_value->nodes[child].kind == kind) count += 1;
    child = fixture_value->nodes[child].next_sibling;
    guard += 1;
  }
  return count;
}

static w_seed_cst_index first_kind(const fixture *fixture_value,
                                   w_seed_cst_kind kind) {
  for (size_t index = 0; index < fixture_value->result.node_count; index += 1) {
    if (fixture_value->nodes[index].kind == kind) return (w_seed_cst_index)index;
  }
  return W_SEED_CST_NONE;
}

static bool contains_kind(const fixture *fixture_value,
                          w_seed_cst_index parent_index,
                          w_seed_cst_kind kind) {
  if (parent_index >= fixture_value->result.node_count) return false;
  w_seed_cst_index child = fixture_value->nodes[parent_index].first_child;
  size_t guard = 0;
  while (child != W_SEED_CST_NONE && guard <= fixture_value->result.node_count) {
    if (fixture_value->nodes[child].kind == kind ||
        contains_kind(fixture_value, child, kind)) {
      return true;
    }
    child = fixture_value->nodes[child].next_sibling;
    guard += 1;
  }
  return false;
}

static w_seed_cst_index direct_child_after(const fixture *fixture_value,
                                           w_seed_cst_index parent_index,
                                           w_seed_cst_kind kind,
                                           size_t offset) {
  if (parent_index >= fixture_value->result.node_count) return W_SEED_CST_NONE;
  w_seed_cst_index child = fixture_value->nodes[parent_index].first_child;
  size_t guard = 0;
  size_t seen = 0;
  while (child != W_SEED_CST_NONE && guard <= fixture_value->result.node_count) {
    if (fixture_value->nodes[child].kind == kind) {
      if (seen == offset) return child;
      seen += 1;
    }
    child = fixture_value->nodes[child].next_sibling;
    guard += 1;
  }
  return W_SEED_CST_NONE;
}

static bool node_span_text(const fixture *fixture_value, w_seed_cst_index index,
                           const char *text) {
  if (index >= fixture_value->result.node_count) return false;
  const w_seed_span span = fixture_value->nodes[index].raw_span;
  const size_t length = strlen(text);
  if (span.end_byte < span.start_byte || span.end_byte - span.start_byte != length ||
      span.end_byte > fixture_value->source.bytes.length) {
    return false;
  }
  return length == 0 ||
         memcmp(fixture_value->source.bytes.data + span.start_byte, text, length) ==
             0;
}

static bool has_direct_text(const fixture *fixture_value,
                            w_seed_cst_index parent_index,
                            w_seed_cst_kind kind, const char *text) {
  if (parent_index >= fixture_value->result.node_count) return false;
  w_seed_cst_index child = fixture_value->nodes[parent_index].first_child;
  size_t guard = 0;
  while (child != W_SEED_CST_NONE && guard <= fixture_value->result.node_count) {
    if (fixture_value->nodes[child].kind == kind &&
        node_span_text(fixture_value, child, text)) {
      return true;
    }
    child = fixture_value->nodes[child].next_sibling;
    guard += 1;
  }
  return false;
}

static bool has_issue(const fixture *fixture_value,
                      w_seed_parse_issue_kind kind);

static size_t count_kind(const fixture *fixture_value, w_seed_cst_kind kind) {
  size_t count = 0;
  for (size_t index = 0; index < fixture_value->result.node_count; index += 1) {
    if (fixture_value->nodes[index].kind == kind) count += 1;
  }
  return count;
}

static bool test_for_control_shapes(void) {
  static const char f0_text[] =
      "fn scan(rows:Rows){scanRows:for ref row in rows{for value in row{"
      "if value==0{continue scanRows} if value>31{break scanRows}"
      " consume(value)}}}\n";
  fixture value;
  CHECK(fixture_init(&value, f0_text,
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(value.result.issue_count == 0);
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));
  CHECK(count_kind(&value, W_SEED_CST_FOR_STATEMENT) == 2);
  CHECK(count_kind(&value, W_SEED_CST_LABEL) == 1);
  const w_seed_cst_index label = first_kind(&value, W_SEED_CST_LABEL);
  CHECK(label != W_SEED_CST_NONE);
  const w_seed_cst_index outer_for =
      direct_child_after(&value, label, W_SEED_CST_FOR_STATEMENT, 0);
  CHECK(outer_for != W_SEED_CST_NONE);
  CHECK(has_direct_text(&value, outer_for, W_SEED_CST_WORD, "ref"));
  const w_seed_cst_index outer_block =
      direct_child_after(&value, outer_for, W_SEED_CST_BLOCK, 0);
  CHECK(outer_block != W_SEED_CST_NONE);
  const w_seed_cst_index nested_for =
      direct_child_after(&value, outer_block, W_SEED_CST_FOR_STATEMENT, 0);
  CHECK(nested_for != W_SEED_CST_NONE);
  const w_seed_cst_index continue_node =
      first_kind(&value, W_SEED_CST_CONTINUE_STATEMENT);
  const w_seed_cst_index break_node = first_kind(&value, W_SEED_CST_BREAK_STATEMENT);
  CHECK(continue_node != W_SEED_CST_NONE);
  CHECK(break_node != W_SEED_CST_NONE);
  CHECK(node_span_text(&value, continue_node, "continue scanRows"));
  CHECK(node_span_text(&value, break_node, "break scanRows"));

  fixture repeated;
  CHECK(fixture_init(&repeated, f0_text,
                     sizeof(repeated.nodes) / sizeof(repeated.nodes[0]),
                     sizeof(repeated.issues) / sizeof(repeated.issues[0])));
  CHECK(memcmp(&value.result, &repeated.result, sizeof(value.result)) == 0);
  CHECK(memcmp(value.nodes, repeated.nodes,
               value.result.node_count * sizeof(value.nodes[0])) == 0);
  CHECK(memcmp(value.issues, repeated.issues,
               value.result.issue_count * sizeof(value.issues[0])) == 0);

  static const char witness_text[] =
      "fn scan(rows:Rows){assembleWord:{scanRows:for ref row in rows{"
      "for value in row{if value==0{continue scanRows}"
      " if value>31{break assembleWord}}}}}\n";
  fixture witness;
  CHECK(fixture_init(&witness, witness_text,
                     sizeof(witness.nodes) / sizeof(witness.nodes[0]),
                     sizeof(witness.issues) / sizeof(witness.issues[0])));
  CHECK(witness.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(witness.result.issue_count == 0);
  CHECK(check_leaf_partition(&witness));
  CHECK(check_tree_links(&witness));
  CHECK(count_kind(&witness, W_SEED_CST_LABEL) == 2);
  CHECK(count_kind(&witness, W_SEED_CST_FOR_STATEMENT) == 2);
  const w_seed_cst_index outer_label = first_kind(&witness, W_SEED_CST_LABEL);
  CHECK(outer_label != W_SEED_CST_NONE);
  const w_seed_cst_index labeled_block =
      direct_child_after(&witness, outer_label, W_SEED_CST_BLOCK, 0);
  CHECK(labeled_block != W_SEED_CST_NONE);
  const w_seed_cst_index inner_label =
      direct_child_after(&witness, labeled_block, W_SEED_CST_LABEL, 0);
  CHECK(inner_label != W_SEED_CST_NONE);
  CHECK(direct_child_after(&witness, inner_label, W_SEED_CST_FOR_STATEMENT, 0) !=
        W_SEED_CST_NONE);
  const w_seed_cst_index witness_break =
      first_kind(&witness, W_SEED_CST_BREAK_STATEMENT);
  CHECK(witness_break != W_SEED_CST_NONE);
  CHECK(node_span_text(&witness, witness_break, "break assembleWord"));
  return true;
}

static bool test_for_markers_and_iterables(void) {
  static const char marker_text[] =
      "fn markers(rows:Rows){for ref row in rows{}for inout item in rows{}"
      "for copy value in rows{}}\n";
  fixture markers;
  CHECK(fixture_init(&markers, marker_text,
                     sizeof(markers.nodes) / sizeof(markers.nodes[0]),
                     sizeof(markers.issues) / sizeof(markers.issues[0])));
  CHECK(markers.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(markers.result.issue_count == 0);
  CHECK(count_kind(&markers, W_SEED_CST_FOR_STATEMENT) == 3);
  CHECK(check_leaf_partition(&markers));
  CHECK(check_tree_links(&markers));
  const w_seed_cst_index marker_block = first_kind(&markers, W_SEED_CST_BLOCK);
  CHECK(marker_block != W_SEED_CST_NONE);
  const w_seed_cst_index ref_loop =
      direct_child_after(&markers, marker_block, W_SEED_CST_FOR_STATEMENT, 0);
  const w_seed_cst_index inout_loop =
      direct_child_after(&markers, marker_block, W_SEED_CST_FOR_STATEMENT, 1);
  const w_seed_cst_index copy_loop =
      direct_child_after(&markers, marker_block, W_SEED_CST_FOR_STATEMENT, 2);
  CHECK(ref_loop != W_SEED_CST_NONE);
  CHECK(inout_loop != W_SEED_CST_NONE);
  CHECK(copy_loop != W_SEED_CST_NONE);
  CHECK(has_direct_text(&markers, ref_loop, W_SEED_CST_WORD, "ref"));
  CHECK(has_direct_text(&markers, inout_loop, W_SEED_CST_WORD, "inout"));
  CHECK(has_direct_text(&markers, copy_loop, W_SEED_CST_WORD, "copy"));

  fixture expression;
  CHECK(fixture_init(&expression,
                     "fn expr(rows:Rows,flags:Flags){for row in rows in flags{}"
                     "for value in (rows[0]){}}\n",
                     sizeof(expression.nodes) / sizeof(expression.nodes[0]),
                     sizeof(expression.issues) / sizeof(expression.issues[0])));
  CHECK(expression.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(expression.result.issue_count == 0);
  CHECK(count_kind(&expression, W_SEED_CST_FOR_STATEMENT) == 2);
  CHECK(check_leaf_partition(&expression));
  CHECK(check_tree_links(&expression));
  return true;
}

static bool test_for_control_recovery(void) {
  static const struct {
    const char *text;
    w_seed_parse_issue_kind issue;
  } recovered[] = {
      {"fn f(rows:Rows){for ref in rows{}}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(rows:Rows){for in rows{}}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(rows:Rows){for ref row rows{}}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(rows:Rows){for ref row in {}}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(rows:Rows){for ref row in rows}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn f(rows:Rows){for ref row in rows{}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
  };
  for (size_t index = 0; index < sizeof(recovered) / sizeof(recovered[0]);
       index += 1) {
    fixture value;
    CHECK(fixture_init(&value, recovered[index].text,
                       sizeof(value.nodes) / sizeof(value.nodes[0]),
                       sizeof(value.issues) / sizeof(value.issues[0])));
    CHECK(value.result.status == W_SEED_PARSE_RECOVERED);
    CHECK(value.result.issue_count >= 1);
    CHECK(value.issues[0].kind == recovered[index].issue);
    CHECK(check_leaf_partition(&value));
    CHECK(check_tree_links(&value));
  }

  fixture binder_recovery;
  CHECK(fixture_init(&binder_recovery, "fn f(rows:Rows){for ref in rows{}}\n",
                     sizeof(binder_recovery.nodes) /
                         sizeof(binder_recovery.nodes[0]),
                     sizeof(binder_recovery.issues) /
                         sizeof(binder_recovery.issues[0])));
  CHECK(binder_recovery.result.status == W_SEED_PARSE_RECOVERED);
  const w_seed_cst_index recovered_for =
      first_kind(&binder_recovery, W_SEED_CST_FOR_STATEMENT);
  CHECK(recovered_for != W_SEED_CST_NONE);
  CHECK(direct_child_after(&binder_recovery, recovered_for, W_SEED_CST_BLOCK, 0) !=
        W_SEED_CST_NONE);
  CHECK(check_leaf_partition(&binder_recovery));
  CHECK(check_tree_links(&binder_recovery));

  fixture unsupported;
  CHECK(fixture_init(&unsupported, "fn f(rows:Rows){outer:while rows{}}\n",
                     sizeof(unsupported.nodes) / sizeof(unsupported.nodes[0]),
                     sizeof(unsupported.issues) / sizeof(unsupported.issues[0])));
  CHECK(unsupported.result.status == W_SEED_PARSE_FATAL);
  CHECK(unsupported.result.issue_count == 1);
  CHECK(unsupported.issues[0].kind == W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
  CHECK(check_leaf_partition(&unsupported));
  CHECK(check_tree_links(&unsupported));

  fixture root;
  CHECK(fixture_init(&root, "for row in rows{}\n",
                     sizeof(root.nodes) / sizeof(root.nodes[0]),
                     sizeof(root.issues) / sizeof(root.issues[0])));
  CHECK(root.result.status == W_SEED_PARSE_FATAL);
  CHECK(root.result.issue_count == 1);
  CHECK(root.issues[0].kind == W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
  CHECK(check_leaf_partition(&root));
  CHECK(check_tree_links(&root));
  static const char *const unsupported_markers[] = {
      "fn f(rows:Rows){for async value in rows{}}\n",
      "fn f(rows:Rows){for await value in rows{}}\n",
      "fn f(rows:Rows){for try await value in rows{}}\n",
      "fn f(rows:Rows){for take value in rows{}}\n",
  };
  for (size_t index = 0;
       index < sizeof(unsupported_markers) / sizeof(unsupported_markers[0]);
       index += 1) {
    fixture marker;
    CHECK(fixture_init(&marker, unsupported_markers[index],
                       sizeof(marker.nodes) / sizeof(marker.nodes[0]),
                       sizeof(marker.issues) / sizeof(marker.issues[0])));
    CHECK(marker.result.status == W_SEED_PARSE_FATAL);
    CHECK(marker.result.issue_count == 1);
    CHECK(marker.issues[0].kind == W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    CHECK(check_leaf_partition(&marker));
    CHECK(check_tree_links(&marker));
  }
  fixture take_iterable;
  CHECK(fixture_init(&take_iterable, "fn f(rows:Rows){for row in take rows{}}\n",
                     sizeof(take_iterable.nodes) /
                         sizeof(take_iterable.nodes[0]),
                     sizeof(take_iterable.issues) /
                         sizeof(take_iterable.issues[0])));
  CHECK(take_iterable.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(take_iterable.result.issue_count == 0);
  CHECK(check_leaf_partition(&take_iterable));
  CHECK(check_tree_links(&take_iterable));
  return true;
}

static bool test_phase2_declaration_tree(void) {
  static const char text[] =
      "import {Command,Result} from command\n"
      "export struct FormatCase {value:String expected:String}\n"
      "export fn namedCall(command:Command,named audit:Audit):String throws KitchenError {"
      "return command}\n"
      "test \"fixture\" for namedCall {expect command == audit}\n";
  fixture value;
  CHECK(fixture_init(&value, text,
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(value.result.issue_count == 0);
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));

  const w_seed_cst_index import = first_kind(&value, W_SEED_CST_IMPORT);
  CHECK(import != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&value, import, W_SEED_CST_IMPORT_ITEM) == 2);
  const w_seed_cst_index structure = first_kind(&value, W_SEED_CST_STRUCT);
  CHECK(structure != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&value, structure, W_SEED_CST_FIELD) == 2);

  const w_seed_cst_index function = first_kind(&value, W_SEED_CST_FUNCTION);
  CHECK(function != W_SEED_CST_NONE);
  const w_seed_cst_index block = direct_child_after(&value, function,
                                                    W_SEED_CST_BLOCK, 0);
  CHECK(block != W_SEED_CST_NONE);
  const char *throws_text = strstr(text, "throws KitchenError");
  CHECK(throws_text != NULL);
  CHECK((size_t)(throws_text - text) < value.nodes[block].raw_span.start_byte);
  CHECK(value.nodes[function].raw_span.end_byte ==
        value.nodes[block].raw_span.end_byte);

  const w_seed_cst_index test = first_kind(&value, W_SEED_CST_TEST);
  CHECK(test != W_SEED_CST_NONE);
  CHECK(contains_kind(&value, test, W_SEED_CST_EXPECT_STATEMENT));
  const w_seed_cst_index expect = first_kind(&value, W_SEED_CST_EXPECT_STATEMENT);
  CHECK(expect != W_SEED_CST_NONE);
  const w_seed_cst_index expected_expression =
      direct_child_after(&value, expect, W_SEED_CST_EXPRESSION, 0);
  CHECK(expected_expression != W_SEED_CST_NONE);
  const char *comparison = strstr(text, "command == audit");
  CHECK(comparison != NULL);
  CHECK(value.nodes[expected_expression].raw_span.start_byte ==
        (size_t)(comparison - text));
  CHECK(value.nodes[expected_expression].raw_span.end_byte ==
        (size_t)(comparison - text) + strlen("command == audit"));
  const char *test_body = strstr(text, "test \"fixture\" for namedCall {");
  CHECK(test_body != NULL);
  const char *expect_text = strstr(test_body, "expect command == audit");
  CHECK(expect_text != NULL);
  CHECK(value.nodes[expect].raw_span.start_byte ==
        (size_t)(expect_text - text));
  CHECK(value.nodes[expect].raw_span.end_byte ==
        value.nodes[expected_expression].raw_span.end_byte);
  CHECK(value.parser.in_test == false);
  return true;
}

static bool test_borrow_clause_shapes(void) {
  static const char text[] =
      "fn pick(primary:ref S,fallback:ref S):view S throws E "
      "borrows(0:[fallback,primary],1:[1,]){return primary}\n";
  fixture value;
  CHECK(fixture_init(&value, text,
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(value.result.issue_count == 0);
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));

  const w_seed_cst_index function = first_kind(&value, W_SEED_CST_FUNCTION);
  CHECK(function != W_SEED_CST_NONE);
  const w_seed_cst_index parameters =
      direct_child_after(&value, function, W_SEED_CST_PARAMETER_LIST, 0);
  const w_seed_cst_index return_type =
      direct_child_after(&value, function, W_SEED_CST_RETURN_TYPE, 0);
  const w_seed_cst_index clause =
      direct_child_after(&value, function, W_SEED_CST_BORROW_CLAUSE, 0);
  const w_seed_cst_index block =
      direct_child_after(&value, function, W_SEED_CST_BLOCK, 0);
  CHECK(parameters != W_SEED_CST_NONE);
  CHECK(return_type != W_SEED_CST_NONE);
  CHECK(clause != W_SEED_CST_NONE);
  CHECK(block != W_SEED_CST_NONE);
  CHECK(value.nodes[parameters].raw_span.start_byte <
        value.nodes[return_type].raw_span.start_byte);
  CHECK(value.nodes[return_type].raw_span.start_byte <
        value.nodes[clause].raw_span.start_byte);
  CHECK(value.nodes[clause].raw_span.start_byte <
        value.nodes[block].raw_span.start_byte);
  CHECK(value.nodes[function].raw_span.end_byte ==
        value.nodes[block].raw_span.end_byte);
  CHECK(node_span_text(&value, clause,
                       "borrows(0:[fallback,primary],1:[1,])"));
  CHECK(count_direct_kind(&value, clause, W_SEED_CST_BORROW_PAIR) == 2);

  const w_seed_cst_index first_pair =
      direct_child_after(&value, clause, W_SEED_CST_BORROW_PAIR, 0);
  const w_seed_cst_index second_pair =
      direct_child_after(&value, clause, W_SEED_CST_BORROW_PAIR, 1);
  CHECK(first_pair != W_SEED_CST_NONE);
  CHECK(second_pair != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&value, first_pair, W_SEED_CST_SLOT_REF) == 3);
  CHECK(count_direct_kind(&value, second_pair, W_SEED_CST_SLOT_REF) == 2);
  CHECK(node_span_text(
      &value,
      direct_child_after(&value, first_pair, W_SEED_CST_SLOT_REF, 0), "0"));
  CHECK(node_span_text(&value,
                       direct_child_after(&value, first_pair,
                                          W_SEED_CST_SLOT_REF, 1),
                       "fallback"));
  CHECK(node_span_text(&value,
                       direct_child_after(&value, first_pair,
                                          W_SEED_CST_SLOT_REF, 2),
                       "primary"));
  CHECK(node_span_text(
      &value,
      direct_child_after(&value, second_pair, W_SEED_CST_SLOT_REF, 0), "1"));
  CHECK(node_span_text(
      &value,
      direct_child_after(&value, second_pair, W_SEED_CST_SLOT_REF, 1), "1"));

  size_t view_types = 0;
  for (size_t index = 0; index < value.result.node_count; index += 1) {
    if (value.nodes[index].kind != W_SEED_CST_TYPE) continue;
    if (has_direct_text(&value, (w_seed_cst_index)index, W_SEED_CST_WORD,
                        "view")) {
      view_types += 1;
    }
  }
  CHECK(view_types == 1);

  static const char view_parameter_text[] =
      "fn pick(primary:view S,fallback:ref S):view S "
      "borrows(0:[fallback,primary]){return primary}\n";
  fixture view_parameter;
  CHECK(fixture_init(&view_parameter, view_parameter_text,
                     sizeof(view_parameter.nodes) /
                         sizeof(view_parameter.nodes[0]),
                     sizeof(view_parameter.issues) /
                         sizeof(view_parameter.issues[0])));
  CHECK(view_parameter.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(view_parameter.result.issue_count == 0);
  CHECK(check_leaf_partition(&view_parameter));
  CHECK(check_tree_links(&view_parameter));
  size_t view_parameter_types = 0;
  for (size_t index = 0; index < view_parameter.result.node_count;
       index += 1) {
    if (view_parameter.nodes[index].kind != W_SEED_CST_TYPE) continue;
    if (has_direct_text(&view_parameter, (w_seed_cst_index)index,
                        W_SEED_CST_WORD, "view")) {
      view_parameter_types += 1;
    }
  }
  CHECK(view_parameter_types == 2);

  fixture repeat;
  CHECK(fixture_init(&repeat, text,
                     sizeof(repeat.nodes) / sizeof(repeat.nodes[0]),
                     sizeof(repeat.issues) / sizeof(repeat.issues[0])));
  CHECK(repeat.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(memcmp(&repeat.result, &value.result, sizeof(value.result)) == 0);
  CHECK(memcmp(repeat.nodes, value.nodes,
               value.result.node_count * sizeof(value.nodes[0])) == 0);
  CHECK(memcmp(repeat.issues, value.issues,
               value.result.issue_count * sizeof(value.issues[0])) == 0);

  static const char lexical_text[] =
      "fn f(primary:ref S):view S borrows(1.5:[unknown],99:[primary,])"
      "{return primary}\n";
  fixture lexical;
  CHECK(fixture_init(&lexical, lexical_text,
                     sizeof(lexical.nodes) / sizeof(lexical.nodes[0]),
                     sizeof(lexical.issues) / sizeof(lexical.issues[0])));
  CHECK(lexical.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(lexical.result.issue_count == 0);
  CHECK(check_leaf_partition(&lexical));
  CHECK(check_tree_links(&lexical));

  static const char duplicate_result_text[] =
      "fn f(primary:ref S):view S borrows(7:[primary],7:[unknown])"
      "{return primary}\n";
  fixture duplicate_result;
  CHECK(fixture_init(&duplicate_result, duplicate_result_text,
                     sizeof(duplicate_result.nodes) /
                         sizeof(duplicate_result.nodes[0]),
                     sizeof(duplicate_result.issues) /
                         sizeof(duplicate_result.issues[0])));
  CHECK(duplicate_result.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(duplicate_result.result.issue_count == 0);
  CHECK(check_leaf_partition(&duplicate_result));
  CHECK(check_tree_links(&duplicate_result));

  static const char comments_text[] =
      "fn f(primary:ref S):view S borrows(0:[/*x*/primary,/*y*/1,],"
      "/*z*/1:[primary,]){return primary}\n";
  fixture comments;
  CHECK(fixture_init(&comments, comments_text,
                     sizeof(comments.nodes) / sizeof(comments.nodes[0]),
                     sizeof(comments.issues) / sizeof(comments.issues[0])));
  CHECK(comments.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(comments.result.issue_count == 0);
  CHECK(check_leaf_partition(&comments));
  CHECK(check_tree_links(&comments));

  fixture contextual;
  CHECK(fixture_init(&contextual, "fn id():S{borrows}\n",
                     sizeof(contextual.nodes) / sizeof(contextual.nodes[0]),
                     sizeof(contextual.issues) / sizeof(contextual.issues[0])));
  CHECK(contextual.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(contextual.result.issue_count == 0);
  CHECK(check_leaf_partition(&contextual));
  CHECK(check_tree_links(&contextual));

  static const struct {
    const char *text;
    w_seed_parse_issue_kind issue;
  } recovered[] = {
      {"fn f(a:ref S):view S borrows(){return a}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(a:ref S):view S borrows(0:[]){return a}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(a:ref S):view S borrows(:[a]){return a}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(a:ref S):view S borrows(0[a]){return a}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(a:ref S):view S borrows(0:a){return a}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(a:ref S):view S borrows(0:[a){return a}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn f(a:ref S):view S borrows(0:[a]{return a}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn f(a:ref S):view S borrows(0:[a 1]){return a}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn f(a:ref S):view S borrows(0:[a])throws E{return a}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn f(a:ref S):view S borrows(0:[a])borrows(0:[a]){return a}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn f(a:ref S):view{return a}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
  };
  for (size_t index = 0;
       index < sizeof(recovered) / sizeof(recovered[0]); index += 1) {
    fixture malformed;
    CHECK(fixture_init(&malformed, recovered[index].text,
                       sizeof(malformed.nodes) / sizeof(malformed.nodes[0]),
                       sizeof(malformed.issues) / sizeof(malformed.issues[0])));
    CHECK(malformed.result.status == W_SEED_PARSE_RECOVERED);
    CHECK(malformed.result.issue_count >= 1);
    CHECK(has_issue(&malformed, recovered[index].issue));
    CHECK(check_leaf_partition(&malformed));
    CHECK(check_tree_links(&malformed));
  }

  fixture after_body;
  CHECK(fixture_init(&after_body,
                     "fn f(a:ref S):view S{return a}borrows(0:[a])\n",
                     sizeof(after_body.nodes) / sizeof(after_body.nodes[0]),
                     sizeof(after_body.issues) / sizeof(after_body.issues[0])));
  CHECK(after_body.result.status == W_SEED_PARSE_RECOVERED);
  CHECK(after_body.result.issue_count >= 1);
  CHECK(check_leaf_partition(&after_body));
  CHECK(check_tree_links(&after_body));

  static const char source_order_a[] =
      "fn pick(primary:ref S,fallback:ref S):view S "
      "borrows(0:[fallback,primary],1:[1]){return primary}\n";
  static const char source_order_b[] =
      "fn pick(primary:ref S,fallback:ref S):view S "
      "borrows(0:[primary,fallback],1:[1]){return primary}\n";
  fixture source_a;
  fixture source_b;
  CHECK(fixture_init(&source_a, source_order_a,
                     sizeof(source_a.nodes) / sizeof(source_a.nodes[0]),
                     sizeof(source_a.issues) / sizeof(source_a.issues[0])));
  CHECK(fixture_init(&source_b, source_order_b,
                     sizeof(source_b.nodes) / sizeof(source_b.nodes[0]),
                     sizeof(source_b.issues) / sizeof(source_b.issues[0])));
  CHECK(source_a.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(source_b.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(source_a.result.issue_count == 0);
  CHECK(source_b.result.issue_count == 0);
  CHECK(check_leaf_partition(&source_a));
  CHECK(check_leaf_partition(&source_b));
  CHECK(check_tree_links(&source_a));
  CHECK(check_tree_links(&source_b));
  const w_seed_cst_index source_a_clause =
      first_kind(&source_a, W_SEED_CST_BORROW_CLAUSE);
  const w_seed_cst_index source_b_clause =
      first_kind(&source_b, W_SEED_CST_BORROW_CLAUSE);
  const w_seed_cst_index source_a_pair =
      direct_child_after(&source_a, source_a_clause,
                         W_SEED_CST_BORROW_PAIR, 0);
  const w_seed_cst_index source_b_pair =
      direct_child_after(&source_b, source_b_clause,
                         W_SEED_CST_BORROW_PAIR, 0);
  CHECK(node_span_text(&source_a, source_a_pair, "0:[fallback,primary]"));
  CHECK(node_span_text(&source_b, source_b_pair, "0:[primary,fallback]"));
  CHECK(memcmp(source_a.nodes, source_b.nodes,
               source_a.result.node_count * sizeof(source_a.nodes[0])) != 0);

  static const char pair_order_a[] =
      "fn pick(primary:ref S,fallback:ref S):view S "
      "borrows(0:[fallback,primary],1:[1]){return primary}\n";
  static const char pair_order_b[] =
      "fn pick(primary:ref S,fallback:ref S):view S "
      "borrows(1:[1],0:[fallback,primary]){return primary}\n";
  fixture pair_a;
  fixture pair_b;
  CHECK(fixture_init(&pair_a, pair_order_a,
                     sizeof(pair_a.nodes) / sizeof(pair_a.nodes[0]),
                     sizeof(pair_a.issues) / sizeof(pair_a.issues[0])));
  CHECK(fixture_init(&pair_b, pair_order_b,
                     sizeof(pair_b.nodes) / sizeof(pair_b.nodes[0]),
                     sizeof(pair_b.issues) / sizeof(pair_b.issues[0])));
  CHECK(pair_a.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(pair_b.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(pair_a.result.issue_count == 0);
  CHECK(pair_b.result.issue_count == 0);
  CHECK(check_leaf_partition(&pair_a));
  CHECK(check_leaf_partition(&pair_b));
  CHECK(check_tree_links(&pair_a));
  CHECK(check_tree_links(&pair_b));
  const w_seed_cst_index pair_a_clause =
      first_kind(&pair_a, W_SEED_CST_BORROW_CLAUSE);
  const w_seed_cst_index pair_b_clause =
      first_kind(&pair_b, W_SEED_CST_BORROW_CLAUSE);
  CHECK(node_span_text(
      &pair_a,
      direct_child_after(&pair_a, pair_a_clause, W_SEED_CST_BORROW_PAIR, 0),
      "0:[fallback,primary]"));
  CHECK(node_span_text(
      &pair_b,
      direct_child_after(&pair_b, pair_b_clause, W_SEED_CST_BORROW_PAIR, 0),
      "1:[1]"));
  CHECK(memcmp(pair_a.nodes, pair_b.nodes,
               pair_a.result.node_count * sizeof(pair_a.nodes[0])) != 0);
  return true;
}

static bool test_async_function_shapes(void) {
  static const char async_text[] =
      "async fn load(kitchen:Kitchen):Menu throws KitchenError{"
      "return try await kitchen.loadMenu()}\n";
  static const char export_async_text[] =
      "export async fn load(kitchen:Kitchen):Menu throws KitchenError{"
      "return try await kitchen.loadMenu()}\n";
  const char *const clean_texts[] = {async_text, export_async_text};
  for (size_t text_index = 0;
       text_index < sizeof(clean_texts) / sizeof(clean_texts[0]);
       text_index += 1) {
    fixture value;
    CHECK(fixture_init(&value, clean_texts[text_index],
                       sizeof(value.nodes) / sizeof(value.nodes[0]),
                       sizeof(value.issues) / sizeof(value.issues[0])));
    CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
    CHECK(value.result.issue_count == 0);
    CHECK(check_leaf_partition(&value));
    CHECK(check_tree_links(&value));
    const w_seed_cst_index function = first_kind(&value, W_SEED_CST_FUNCTION);
    CHECK(function != W_SEED_CST_NONE);
    CHECK(value.nodes[function].raw_span.start_byte == 0);
    bool async_raw = false;
    w_seed_cst_index function_child = value.nodes[function].first_child;
    while (function_child != W_SEED_CST_NONE) {
      const w_seed_cst_node *child = &value.nodes[function_child];
      if (child->kind == W_SEED_CST_WORD &&
          node_span_text(&value, function_child, "async")) {
        CHECK((child->flags & W_SEED_CST_FLAG_RAW_LEAF) != 0);
        async_raw = true;
      }
      function_child = child->next_sibling;
    }
    CHECK(async_raw);
    if (text_index == 1) {
      CHECK(has_direct_text(&value, function, W_SEED_CST_WORD, "export"));
    }
    const w_seed_cst_index parameters =
        direct_child_after(&value, function, W_SEED_CST_PARAMETER_LIST, 0);
    const w_seed_cst_index return_type =
        direct_child_after(&value, function, W_SEED_CST_RETURN_TYPE, 0);
    const w_seed_cst_index block =
        direct_child_after(&value, function, W_SEED_CST_BLOCK, 0);
    CHECK(parameters != W_SEED_CST_NONE);
    CHECK(return_type != W_SEED_CST_NONE);
    CHECK(block != W_SEED_CST_NONE);
    CHECK(value.nodes[function].raw_span.end_byte ==
          value.nodes[block].raw_span.end_byte);
    CHECK(has_direct_text(&value, function, W_SEED_CST_WORD, "throws"));
    const w_seed_cst_index return_statement =
        direct_child_after(&value, block, W_SEED_CST_RETURN_STATEMENT, 0);
    const w_seed_cst_index expression =
        direct_child_after(&value, return_statement, W_SEED_CST_EXPRESSION, 0);
    CHECK(return_statement != W_SEED_CST_NONE);
    CHECK(expression != W_SEED_CST_NONE);
    bool saw_try = false;
    bool saw_await = false;
    for (size_t node_index = 0; node_index < value.result.node_count;
         node_index += 1) {
      const w_seed_cst_node *node = &value.nodes[node_index];
      if (node->kind != W_SEED_CST_WORD ||
          node->raw_span.start_byte <
              value.nodes[expression].raw_span.start_byte ||
          node->raw_span.end_byte > value.nodes[expression].raw_span.end_byte) {
        continue;
      }
      if (node_span_text(&value, (w_seed_cst_index)node_index, "try")) {
        saw_try = true;
      }
      if (node_span_text(&value, (w_seed_cst_index)node_index, "await")) {
        saw_await = true;
      }
    }
    CHECK(saw_try);
    CHECK(saw_await);

    fixture repeat;
    CHECK(fixture_init(&repeat, clean_texts[text_index],
                       sizeof(repeat.nodes) / sizeof(repeat.nodes[0]),
                       sizeof(repeat.issues) / sizeof(repeat.issues[0])));
    CHECK(repeat.result.status == W_SEED_PARSE_COMPLETE);
    CHECK(repeat.result.node_count == value.result.node_count);
    CHECK(repeat.result.leaf_count == value.result.leaf_count);
    CHECK(memcmp(repeat.nodes, value.nodes,
                 value.result.node_count * sizeof(value.nodes[0])) == 0);
  }

  static const char trivia_text[] =
      "export /*a*/ async /*b*/ fn f(){}\n";
  fixture trivia;
  CHECK(fixture_init(&trivia, trivia_text,
                     sizeof(trivia.nodes) / sizeof(trivia.nodes[0]),
                     sizeof(trivia.issues) / sizeof(trivia.issues[0])));
  CHECK(trivia.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(trivia.result.issue_count == 0);
  CHECK(check_leaf_partition(&trivia));
  CHECK(check_tree_links(&trivia));
  const w_seed_cst_index trivia_function =
      first_kind(&trivia, W_SEED_CST_FUNCTION);
  CHECK(trivia_function != W_SEED_CST_NONE);
  CHECK(trivia.nodes[trivia_function].raw_span.start_byte == 0);
  CHECK(count_direct_kind(&trivia, trivia_function, W_SEED_CST_TRIVIA) >= 2);
  CHECK(has_direct_text(&trivia, trivia_function, W_SEED_CST_TRIVIA, "/*a*/"));
  CHECK(has_direct_text(&trivia, trivia_function, W_SEED_CST_TRIVIA, "/*b*/"));

  static const struct {
    const char *text;
    w_seed_parse_issue_kind issue;
  } recovered[] = {
      {"async fn\n", W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"async fn f(a:T{}\n", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"async fn f(){return 1\n", W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
  };
  for (size_t index = 0; index < sizeof(recovered) / sizeof(recovered[0]);
       index += 1) {
    fixture value;
    CHECK(fixture_init(&value, recovered[index].text,
                       sizeof(value.nodes) / sizeof(value.nodes[0]),
                       sizeof(value.issues) / sizeof(value.issues[0])));
    CHECK(value.result.status == W_SEED_PARSE_RECOVERED);
    CHECK(value.result.issue_count >= 1);
    CHECK(value.issues[0].kind == recovered[index].issue);
    CHECK(check_leaf_partition(&value));
    CHECK(check_tree_links(&value));
  }

  static const char *const stops[] = {
      "async\n",             "async async fn f(){}\n",
      "async struct S {}\n", "async test \"bad\" for f {}\n",
      "async entry(f)\n",    "export async struct S {}\n",
      "static fn f(){}\n",   "const fn f(){}\n",
      "unsafe fn f(){}\n",   "mut fn f(){}\n",
      "take fn f(){}\n",
  };
  for (size_t index = 0; index < sizeof(stops) / sizeof(stops[0]);
       index += 1) {
    fixture value;
    CHECK(fixture_init(&value, stops[index],
                       sizeof(value.nodes) / sizeof(value.nodes[0]),
                       sizeof(value.issues) / sizeof(value.issues[0])));
    CHECK(value.result.status == W_SEED_PARSE_FATAL);
    CHECK(value.result.issue_count == 1);
    CHECK(value.issues[0].kind == W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    CHECK(check_leaf_partition(&value));
    CHECK(check_tree_links(&value));
  }

  fixture reversed_effects;
  CHECK(fixture_init(&reversed_effects,
                     "async fn f(){return await try value()}\n",
                     sizeof(reversed_effects.nodes) /
                         sizeof(reversed_effects.nodes[0]),
                     sizeof(reversed_effects.issues) /
                         sizeof(reversed_effects.issues[0])));
  CHECK(reversed_effects.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(reversed_effects.result.issue_count == 0);
  CHECK(check_leaf_partition(&reversed_effects));
  CHECK(check_tree_links(&reversed_effects));
  return true;
}

static bool test_transaction_shapes(void) {
  static const char transaction_text[] =
      "async fn settle(table:TableId,guest:GuestId):Receipt throws LedgerError{"
      "return try await transaction tx=tableLedger{"
      "let reservation=try await tx.reserve(tableId:table,guestId:guest)"
      "let receipt=try await tx.confirm(reservation:take reservation)"
      "commit receipt}}\n";
  fixture value;
  CHECK(fixture_init(&value, transaction_text,
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(value.result.issue_count == 0);
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));
  CHECK(count_kind(&value, W_SEED_CST_TRANSACTION_EXPRESSION) == 1);
  CHECK(count_kind(&value, W_SEED_CST_COMMIT_STATEMENT) == 1);
  const w_seed_cst_index transaction =
      first_kind(&value, W_SEED_CST_TRANSACTION_EXPRESSION);
  CHECK(transaction != W_SEED_CST_NONE);
  CHECK(has_direct_text(&value, transaction, W_SEED_CST_WORD, "transaction"));
  CHECK(has_direct_text(&value, transaction, W_SEED_CST_WORD, "tx"));
  const w_seed_cst_index transaction_block =
      direct_child_after(&value, transaction, W_SEED_CST_BLOCK, 0);
  CHECK(transaction_block != W_SEED_CST_NONE);
  const w_seed_cst_index commit =
      direct_child_after(&value, transaction_block,
                         W_SEED_CST_COMMIT_STATEMENT, 0);
  CHECK(commit != W_SEED_CST_NONE);
  CHECK(node_span_text(&value, commit, "commit receipt"));
  const w_seed_cst_index commit_expression =
      direct_child_after(&value, commit, W_SEED_CST_EXPRESSION, 0);
  CHECK(commit_expression != W_SEED_CST_NONE);
  CHECK(node_span_text(&value, commit_expression, "receipt"));
  const w_seed_cst_index return_statement =
      first_kind(&value, W_SEED_CST_RETURN_STATEMENT);
  CHECK(return_statement != W_SEED_CST_NONE);
  const w_seed_cst_index return_expression =
      direct_child_after(&value, return_statement, W_SEED_CST_EXPRESSION, 0);
  CHECK(return_expression != W_SEED_CST_NONE);
  CHECK(has_direct_text(&value, return_expression, W_SEED_CST_WORD, "try"));
  CHECK(has_direct_text(&value, return_expression, W_SEED_CST_WORD, "await"));

  fixture repeat;
  CHECK(fixture_init(&repeat, transaction_text,
                     sizeof(repeat.nodes) / sizeof(repeat.nodes[0]),
                     sizeof(repeat.issues) / sizeof(repeat.issues[0])));
  CHECK(repeat.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(repeat.result.node_count == value.result.node_count);
  CHECK(repeat.result.leaf_count == value.result.leaf_count);
  CHECK(memcmp(repeat.nodes, value.nodes,
               value.result.node_count * sizeof(value.nodes[0])) == 0);

  static const struct {
    const char *text;
    size_t transaction_count;
    size_t commit_count;
  } semantic_forms[] = {
      {"fn f(){commit value}\n", 0, 1},
      {"fn f(){commit}\n", 0, 1},
      {"fn f(){return transaction outer=provider{"
       "commit transaction inner=provider{commit value}}}\n",
       2,
       2},
  };
  for (size_t index = 0;
       index < sizeof(semantic_forms) / sizeof(semantic_forms[0]); index += 1) {
    fixture semantic;
    CHECK(fixture_init(&semantic, semantic_forms[index].text,
                       sizeof(semantic.nodes) / sizeof(semantic.nodes[0]),
                       sizeof(semantic.issues) / sizeof(semantic.issues[0])));
    CHECK(semantic.result.status == W_SEED_PARSE_COMPLETE);
    CHECK(semantic.result.issue_count == 0);
    CHECK(check_leaf_partition(&semantic));
    CHECK(check_tree_links(&semantic));
    CHECK(count_kind(&semantic, W_SEED_CST_TRANSACTION_EXPRESSION) ==
          semantic_forms[index].transaction_count);
    CHECK(count_kind(&semantic, W_SEED_CST_COMMIT_STATEMENT) ==
          semantic_forms[index].commit_count);
    const w_seed_cst_index semantic_commit =
        first_kind(&semantic, W_SEED_CST_COMMIT_STATEMENT);
    CHECK(semantic_commit != W_SEED_CST_NONE);
    if (index == 1) {
      CHECK(direct_child_after(&semantic, semantic_commit,
                               W_SEED_CST_EXPRESSION, 0) == W_SEED_CST_NONE);
    }
    if (index == 2) {
      const w_seed_cst_index outer =
          first_kind(&semantic, W_SEED_CST_TRANSACTION_EXPRESSION);
      const w_seed_cst_index outer_block =
          direct_child_after(&semantic, outer, W_SEED_CST_BLOCK, 0);
      CHECK(outer_block != W_SEED_CST_NONE);
      const w_seed_cst_index outer_commit =
          direct_child_after(&semantic, outer_block,
                             W_SEED_CST_COMMIT_STATEMENT, 0);
      CHECK(outer_commit != W_SEED_CST_NONE);
      const w_seed_cst_index outer_expression =
          direct_child_after(&semantic, outer_commit, W_SEED_CST_EXPRESSION, 0);
      CHECK(outer_expression != W_SEED_CST_NONE);
      const w_seed_cst_index inner =
          direct_child_after(&semantic, outer_expression,
                             W_SEED_CST_TRANSACTION_EXPRESSION, 0);
      CHECK(inner != W_SEED_CST_NONE);
      const w_seed_cst_index inner_block =
          direct_child_after(&semantic, inner, W_SEED_CST_BLOCK, 0);
      CHECK(inner_block != W_SEED_CST_NONE);
      CHECK(direct_child_after(&semantic, inner_block,
                               W_SEED_CST_COMMIT_STATEMENT, 0) !=
            W_SEED_CST_NONE);
    }
    fixture semantic_repeat;
    CHECK(fixture_init(&semantic_repeat, semantic_forms[index].text,
                       sizeof(semantic_repeat.nodes) /
                           sizeof(semantic_repeat.nodes[0]),
                       sizeof(semantic_repeat.issues) /
                           sizeof(semantic_repeat.issues[0])));
    CHECK(semantic_repeat.result.status == W_SEED_PARSE_COMPLETE);
    CHECK(semantic_repeat.result.issue_count == 0);
    CHECK(semantic_repeat.result.node_count == semantic.result.node_count);
    CHECK(semantic_repeat.result.leaf_count == semantic.result.leaf_count);
    CHECK(memcmp(semantic_repeat.nodes, semantic.nodes,
                 semantic.result.node_count * sizeof(semantic.nodes[0])) == 0);
  }

  static const struct {
    const char *text;
    w_seed_parse_status status;
    w_seed_parse_issue_kind issue;
  } recovered[] = {
      {"fn f(){return transaction = provider{commit value}}\n",
       W_SEED_PARSE_RECOVERED, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(){return transaction tx provider{commit value}}\n",
       W_SEED_PARSE_RECOVERED, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(){return transaction tx= {commit value}}\n",
       W_SEED_PARSE_RECOVERED, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(){return transaction tx=provider}\n",
       W_SEED_PARSE_RECOVERED, W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn f(){return transaction tx=provider{commit value\n",
       W_SEED_PARSE_RECOVERED, W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn f(){commit value else}\n", W_SEED_PARSE_RECOVERED,
       W_SEED_PARSE_ISSUE_NO_CONTINUATION_OWNER},
  };
  for (size_t index = 0; index < sizeof(recovered) / sizeof(recovered[0]);
       index += 1) {
    fixture malformed;
    CHECK(fixture_init(&malformed, recovered[index].text,
                       sizeof(malformed.nodes) / sizeof(malformed.nodes[0]),
                       sizeof(malformed.issues) / sizeof(malformed.issues[0])));
    CHECK(malformed.result.status == recovered[index].status);
    CHECK(malformed.result.issue_count >= 1);
    CHECK(malformed.issues[0].kind == recovered[index].issue);
    CHECK(check_leaf_partition(&malformed));
    CHECK(check_tree_links(&malformed));
  }

  fixture contract;
  CHECK(fixture_init(&contract,
                     "fn f(){return transaction<.serializable> tx=provider{"
                     "commit value}}\n",
                     sizeof(contract.nodes) / sizeof(contract.nodes[0]),
                     sizeof(contract.issues) / sizeof(contract.issues[0])));
  CHECK(contract.result.status == W_SEED_PARSE_FATAL);
  CHECK(contract.result.issue_count == 1);
  CHECK(contract.issues[0].kind == W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
  CHECK(check_leaf_partition(&contract));
  CHECK(check_tree_links(&contract));
  return true;
}

static bool test_language_lock_shapes(void) {
  static const struct {
    const char *text;
    const char *lock_text;
    const char *target_text;
    const char *binding_text;
    size_t lock_count;
    const char *prefix;
  } positive[] = {
      {"fn snapshot(state:shared Ledger):Ledger{"
       "return lock state as value{copy value}}\n",
       "lock state as value{copy value}", "state ", "value", 1, NULL},
      {"fn snapshot(state:shared Ledger):Ledger{"
       "return await lock state as value{copy value}}\n",
       "lock state as value{copy value}", "state ", "value", 1, "await"},
      {"fn snapshot(state:shared Ledger):Ledger{"
       "return try lock state as value{copy value}}\n",
       "lock state as value{copy value}", "state ", "value", 1, "try"},
      {"fn snapshot(state:shared Ledger):Ledger{"
       "return lock state.current as value{await work()}}\n",
       "lock state.current as value{await work()}", "state.current ",
       "value", 1, NULL},
      {"fn snapshot(state:shared Ledger):Ledger{"
       "return lock (state) as value{copy value}}\n",
       "lock (state) as value{copy value}", "(state) ", "value", 1, NULL},
      {"fn snapshot(state:shared Ledger):Ledger{"
       "return lock state as value{ref value}}\n",
       "lock state as value{ref value}", "state ", "value", 1, NULL},
      {"fn snapshot(state:shared Ledger):Ledger{"
       "return lock state as value{lock state as nested{copy nested}}}\n",
       "lock state as value{lock state as nested{copy nested}}", "state ",
       "value", 2, NULL},
  };
  for (size_t index = 0;
       index < sizeof(positive) / sizeof(positive[0]); index += 1) {
    fixture value;
    CHECK(fixture_init(&value, positive[index].text,
                       sizeof(value.nodes) / sizeof(value.nodes[0]),
                       sizeof(value.issues) / sizeof(value.issues[0])));
    CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
    CHECK(value.result.issue_count == 0);
    CHECK(check_leaf_partition(&value));
    CHECK(check_tree_links(&value));
    CHECK(count_kind(&value, W_SEED_CST_LOCK_EXPRESSION) ==
          positive[index].lock_count);

    const w_seed_cst_index function = first_kind(&value, W_SEED_CST_FUNCTION);
    CHECK(function != W_SEED_CST_NONE);
    const w_seed_cst_index parameters =
        direct_child_after(&value, function, W_SEED_CST_PARAMETER_LIST, 0);
    CHECK(parameters != W_SEED_CST_NONE);
    const w_seed_cst_index parameter =
        direct_child_after(&value, parameters, W_SEED_CST_PARAMETER, 0);
    CHECK(parameter != W_SEED_CST_NONE);
    const w_seed_cst_index type =
        direct_child_after(&value, parameter, W_SEED_CST_TYPE, 0);
    CHECK(type != W_SEED_CST_NONE);
    CHECK(node_span_text(&value, type, "shared Ledger"));
    CHECK(has_direct_text(&value, type, W_SEED_CST_WORD, "shared"));

    const w_seed_cst_index lock =
        first_kind(&value, W_SEED_CST_LOCK_EXPRESSION);
    CHECK(lock != W_SEED_CST_NONE);
    CHECK(node_span_text(&value, lock, positive[index].lock_text));
    CHECK(count_direct_kind(&value, lock, W_SEED_CST_WORD) == 3);
    CHECK(count_direct_kind(&value, lock, W_SEED_CST_EXPRESSION) == 1);
    CHECK(count_direct_kind(&value, lock, W_SEED_CST_BLOCK) == 1);
    const w_seed_cst_index lock_word =
        direct_child_after(&value, lock, W_SEED_CST_WORD, 0);
    const w_seed_cst_index as_word =
        direct_child_after(&value, lock, W_SEED_CST_WORD, 1);
    const w_seed_cst_index binding =
        direct_child_after(&value, lock, W_SEED_CST_WORD, 2);
    const w_seed_cst_index target =
        direct_child_after(&value, lock, W_SEED_CST_EXPRESSION, 0);
    const w_seed_cst_index body =
        direct_child_after(&value, lock, W_SEED_CST_BLOCK, 0);
    CHECK(lock_word != W_SEED_CST_NONE);
    CHECK(as_word != W_SEED_CST_NONE);
    CHECK(binding != W_SEED_CST_NONE);
    CHECK(target != W_SEED_CST_NONE);
    CHECK(body != W_SEED_CST_NONE);
    CHECK(node_span_text(&value, lock_word, "lock"));
    CHECK(node_span_text(&value, as_word, "as"));
    CHECK(node_span_text(&value, target, positive[index].target_text));
    CHECK(node_span_text(&value, binding, positive[index].binding_text));
    CHECK(value.nodes[lock].raw_span.start_byte ==
          value.nodes[lock_word].raw_span.start_byte);
    CHECK(value.nodes[lock_word].raw_span.end_byte <
          value.nodes[target].raw_span.start_byte);
    CHECK(value.nodes[target].raw_span.end_byte <=
          value.nodes[as_word].raw_span.start_byte);
    CHECK(value.nodes[as_word].raw_span.end_byte <
          value.nodes[binding].raw_span.start_byte);
    CHECK(value.nodes[binding].raw_span.end_byte <=
          value.nodes[body].raw_span.start_byte);
    CHECK(value.nodes[lock].raw_span.end_byte ==
          value.nodes[body].raw_span.end_byte);

    const w_seed_cst_index block =
        direct_child_after(&value, function, W_SEED_CST_BLOCK, 0);
    CHECK(block != W_SEED_CST_NONE);
    const w_seed_cst_index return_statement =
        direct_child_after(&value, block, W_SEED_CST_RETURN_STATEMENT, 0);
    CHECK(return_statement != W_SEED_CST_NONE);
    const w_seed_cst_index expression =
        direct_child_after(&value, return_statement, W_SEED_CST_EXPRESSION, 0);
    CHECK(expression != W_SEED_CST_NONE);
    if (positive[index].prefix != NULL) {
      CHECK(has_direct_text(&value, expression, W_SEED_CST_WORD,
                            positive[index].prefix));
    }

    fixture repeat;
    CHECK(fixture_init(&repeat, positive[index].text,
                       sizeof(repeat.nodes) / sizeof(repeat.nodes[0]),
                       sizeof(repeat.issues) / sizeof(repeat.issues[0])));
    CHECK(repeat.result.status == W_SEED_PARSE_COMPLETE);
    CHECK(repeat.result.node_count == value.result.node_count);
    CHECK(repeat.result.leaf_count == value.result.leaf_count);
    CHECK(memcmp(repeat.nodes, value.nodes,
                 value.result.node_count * sizeof(value.nodes[0])) == 0);
  }

  static const struct {
    const char *text;
    w_seed_parse_issue_kind issue;
  } recovered[] = {
      {"fn f(state:shared Ledger):Ledger{"
       "return lock state value{copy value}}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(state:shared Ledger):Ledger{"
       "return lock as value{copy value}}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(state:shared Ledger):Ledger{"
       "return lock state as{copy state}}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(state:shared Ledger):Ledger{"
       "return lock state as value\n}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn f(state:shared Ledger):Ledger{"
       "return lock state as value{copy value}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn f(state:shared Ledger):Ledger{"
       "return lock state as 0{copy value}}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(state:shared Ledger):Ledger{"
       "return lock state as value(copy value)}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn f(state:shared Ledger):Ledger{"
       "return lock state as value as other{copy value}}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
  };
  for (size_t index = 0;
       index < sizeof(recovered) / sizeof(recovered[0]); index += 1) {
    fixture malformed;
    CHECK(fixture_init(&malformed, recovered[index].text,
                       sizeof(malformed.nodes) / sizeof(malformed.nodes[0]),
                       sizeof(malformed.issues) / sizeof(malformed.issues[0])));
    CHECK(malformed.result.status == W_SEED_PARSE_RECOVERED);
    CHECK(malformed.result.issue_count >= 1);
    CHECK(has_issue(&malformed, recovered[index].issue));
    CHECK(check_leaf_partition(&malformed));
    CHECK(check_tree_links(&malformed));
  }

  fixture missing_target;
  CHECK(fixture_init(&missing_target,
                     "fn f(state:shared Ledger):Ledger{"
                     "return lock as value{copy value}}\n",
                     sizeof(missing_target.nodes) /
                         sizeof(missing_target.nodes[0]),
                     sizeof(missing_target.issues) /
                         sizeof(missing_target.issues[0])));
  const w_seed_cst_index missing_lock =
      first_kind(&missing_target, W_SEED_CST_LOCK_EXPRESSION);
  CHECK(missing_lock != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&missing_target, missing_lock,
                          W_SEED_CST_EXPRESSION) == 0);
  CHECK(count_direct_kind(&missing_target, missing_lock,
                          W_SEED_CST_MISSING) >= 1);
  CHECK(check_leaf_partition(&missing_target));
  CHECK(check_tree_links(&missing_target));

  fixture extra_as;
  CHECK(fixture_init(&extra_as,
                     "fn f(state:shared Ledger):Ledger{"
                     "return lock state as value as other{copy value}}\n",
                     sizeof(extra_as.nodes) / sizeof(extra_as.nodes[0]),
                     sizeof(extra_as.issues) / sizeof(extra_as.issues[0])));
  CHECK(node_span_text(
      &extra_as, first_kind(&extra_as, W_SEED_CST_LOCK_EXPRESSION),
      "lock state as value "));
  CHECK(check_leaf_partition(&extra_as));
  CHECK(check_tree_links(&extra_as));
  return true;
}

static bool test_phase2_parameter_and_argument_shapes(void) {
  static const char text[] =
      "fn inspect(named:T,named audit:Audit,external internal:T,_ internal:T):T {"
      "return inspect(named:value,audit:value)}\n";
  fixture value;
  CHECK(fixture_init(&value, text,
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));
  const w_seed_cst_index function = first_kind(&value, W_SEED_CST_FUNCTION);
  CHECK(function != W_SEED_CST_NONE);
  const w_seed_cst_index parameters =
      direct_child_after(&value, function, W_SEED_CST_PARAMETER_LIST, 0);
  CHECK(parameters != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&value, parameters, W_SEED_CST_PARAMETER) == 4);
  const char *parameter_texts[] = {"named:T", "named audit:Audit",
                                   "external internal:T", "_ internal:T"};
  for (size_t index = 0; index < 4; index += 1) {
    const w_seed_cst_index parameter =
        direct_child_after(&value, parameters, W_SEED_CST_PARAMETER, index);
    CHECK(parameter != W_SEED_CST_NONE);
    CHECK(node_span_text(&value, parameter, parameter_texts[index]));
  }
  size_t argument_count = 0;
  for (size_t index = 0; index < value.result.node_count; index += 1) {
    if (value.nodes[index].kind != W_SEED_CST_ARGUMENT) continue;
    CHECK(argument_count < 2);
    const char *argument_text = argument_count == 0 ? "named:value" : "audit:value";
    CHECK(node_span_text(&value, (w_seed_cst_index)index, argument_text));
    argument_count += 1;
  }
  CHECK(argument_count == 2);
  return true;
}

static bool test_phase2_parameter_requirements(void) {
  static const char text[] =
      "fn requirements(value:ref T,cursor:inout T,owner:take T,limit:const T):T {"
      "return value}\n";
  fixture value;
  CHECK(fixture_init(&value, text,
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(value.result.issue_count == 0);
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));
  const w_seed_cst_index function = first_kind(&value, W_SEED_CST_FUNCTION);
  CHECK(function != W_SEED_CST_NONE);
  const w_seed_cst_index parameters =
      direct_child_after(&value, function, W_SEED_CST_PARAMETER_LIST, 0);
  CHECK(parameters != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&value, parameters, W_SEED_CST_PARAMETER) == 4);
  const char *requirements[] = {"value:ref T", "cursor:inout T", "owner:take T",
                                "limit:const T"};
  for (size_t index = 0; index < 4; index += 1) {
    const w_seed_cst_index parameter =
        direct_child_after(&value, parameters, W_SEED_CST_PARAMETER, index);
    CHECK(parameter != W_SEED_CST_NONE);
    CHECK(node_span_text(&value, parameter, requirements[index]));
  }
  return true;
}

static bool test_phase2_prefix_forms(void) {
  static const char *const prefixes[] = {"copy", "take", "pin", "inout", "ref"};
  for (size_t index = 0; index < sizeof(prefixes) / sizeof(prefixes[0]); index += 1) {
    char text[96];
    const int written = snprintf(text, sizeof(text), "fn f(){%s value}\n", prefixes[index]);
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

static bool test_phase2_fatal_boundaries(void) {
  static const char *const texts[] = {
      "fn f(){}\nimport {x} from module.path\n",
      "export test \"unsupported\" for f {}\n",
      "fn f(){expect value == other}\n",
      "import {x} module.path\n",
      "entry(f)\nstruct S {}\n",
      "entry(f)\ntest \"late\" for f {}\n",
      "entry(f)\nexport struct S {}\n",
      "entry(f)\nimport {x} from module.path\n",
      "const value:T\n",
      "take value\n",
  };
  for (size_t index = 0; index < sizeof(texts) / sizeof(texts[0]); index += 1) {
    fixture value;
    CHECK(fixture_init(&value, texts[index],
                       sizeof(value.nodes) / sizeof(value.nodes[0]),
                       sizeof(value.issues) / sizeof(value.issues[0])));
    CHECK(value.result.status == W_SEED_PARSE_FATAL);
    CHECK(value.result.issue_count == 1);
    CHECK(value.issues[0].kind == W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    CHECK(check_leaf_partition(&value));
    CHECK(check_tree_links(&value));
    CHECK(value.parser.in_test == false);
  }

  fixture value;
  CHECK(fixture_init(&value, "test \"bad\" for f {foreign c { body }}\n",
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_FATAL);
  CHECK(value.result.issue_count == 1);
  CHECK(value.issues[0].kind == W_SEED_PARSE_ISSUE_FOREIGN_UNSUPPORTED);
  CHECK(value.parser.in_test == false);
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));
  return true;
}

static bool test_phase2_recovery_mutations(void) {
  static const char *const texts[] = {
      "fn f(a T){}\n",
      "fn f(a:T{}\n",
      "fn f(){return 1\n",
      "struct S {:T}\n",
  };
  for (size_t index = 0; index < sizeof(texts) / sizeof(texts[0]); index += 1) {
    fixture value;
    CHECK(fixture_init(&value, texts[index],
                       sizeof(value.nodes) / sizeof(value.nodes[0]),
                       sizeof(value.issues) / sizeof(value.issues[0])));
    CHECK(value.result.status == W_SEED_PARSE_RECOVERED);
    CHECK(value.result.issue_count >= 1);
    CHECK(check_leaf_partition(&value));
    CHECK(check_tree_links(&value));
  }
  return true;
}

static bool same_parse(const fixture *left, const fixture *right) {
  CHECK(memcmp(&left->result, &right->result, sizeof(left->result)) == 0);
  CHECK(memcmp(left->nodes, right->nodes,
               left->result.node_count * sizeof(left->nodes[0])) == 0);
  CHECK(memcmp(left->issues, right->issues,
               left->result.issue_count * sizeof(left->issues[0])) == 0);
  return true;
}

static bool check_complete_shape(const fixture *value) {
  CHECK(value->result.status == W_SEED_PARSE_COMPLETE);
  CHECK(value->result.issue_count == 0);
  CHECK(check_leaf_partition(value));
  CHECK(check_tree_links(value));
  return true;
}

static bool test_allocator_block_shapes(void) {
  static const char anonymous_text[] =
      "fn stage(allocator memory:ref Allocator,title:ref String,"
      "dishes menuDishes:ref Array<String>):MenuSnapshot{"
      "allocator .fixed<capacity:64<iec.KiB>>{"
      "let snapshot=stage(ref title,dishes:ref dishes)}}\n";
  fixture anonymous;
  CHECK(fixture_init(&anonymous, anonymous_text,
                     sizeof(anonymous.nodes) / sizeof(anonymous.nodes[0]),
                     sizeof(anonymous.issues) / sizeof(anonymous.issues[0])));
  CHECK(check_complete_shape(&anonymous));
  CHECK(count_kind(&anonymous, W_SEED_CST_ALLOCATOR_BLOCK) == 1);
  const w_seed_cst_index anonymous_allocator =
      first_kind(&anonymous, W_SEED_CST_ALLOCATOR_BLOCK);
  CHECK(anonymous_allocator != W_SEED_CST_NONE);
  const w_seed_cst_index anonymous_keyword = direct_child_after(
      &anonymous, anonymous_allocator, W_SEED_CST_WORD, 0);
  const w_seed_cst_index anonymous_plan = direct_child_after(
      &anonymous, anonymous_allocator, W_SEED_CST_EXPRESSION, 0);
  const w_seed_cst_index anonymous_body = direct_child_after(
      &anonymous, anonymous_allocator, W_SEED_CST_BLOCK, 0);
  CHECK(anonymous_keyword != W_SEED_CST_NONE);
  CHECK(anonymous_plan != W_SEED_CST_NONE);
  CHECK(anonymous_body != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&anonymous, anonymous_allocator, W_SEED_CST_WORD) == 1);
  CHECK(node_span_text(&anonymous, anonymous_keyword, "allocator"));
  CHECK(node_span_text(&anonymous, anonymous_plan,
                       ".fixed<capacity:64<iec.KiB>>"));
  CHECK(anonymous.nodes[anonymous_allocator].raw_span.start_byte ==
        anonymous.nodes[anonymous_keyword].raw_span.start_byte);
  CHECK(anonymous.nodes[anonymous_allocator].raw_span.end_byte >=
        anonymous.nodes[anonymous_body].raw_span.end_byte);
  CHECK(count_direct_kind(&anonymous, anonymous_body,
                          W_SEED_CST_LET_STATEMENT) == 1);

  fixture anonymous_repeat;
  CHECK(fixture_init(&anonymous_repeat, anonymous_text,
                     sizeof(anonymous_repeat.nodes) /
                         sizeof(anonymous_repeat.nodes[0]),
                     sizeof(anonymous_repeat.issues) /
                         sizeof(anonymous_repeat.issues[0])));
  CHECK(same_parse(&anonymous, &anonymous_repeat));

  static const char named_text[] =
      "fn caller(allocator memory:ref Allocator,title:ref String){"
      "allocator scratch:.fixed<capacity:64<iec.KiB>>{"
      "let snapshot=stage(allocator:ref memory,ref title)}}\n";
  fixture named;
  CHECK(fixture_init(&named, named_text,
                     sizeof(named.nodes) / sizeof(named.nodes[0]),
                     sizeof(named.issues) / sizeof(named.issues[0])));
  CHECK(check_complete_shape(&named));
  const w_seed_cst_index named_allocator =
      first_kind(&named, W_SEED_CST_ALLOCATOR_BLOCK);
  CHECK(named_allocator != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&named, named_allocator, W_SEED_CST_WORD) == 2);
  CHECK(has_direct_text(&named, named_allocator, W_SEED_CST_WORD, "allocator"));
  CHECK(has_direct_text(&named, named_allocator, W_SEED_CST_WORD, "scratch"));
  CHECK(has_direct_text(&named, named_allocator, W_SEED_CST_PUNCTUATION, ":"));
  const w_seed_cst_index named_plan = direct_child_after(
      &named, named_allocator, W_SEED_CST_EXPRESSION, 0);
  const w_seed_cst_index named_body = direct_child_after(
      &named, named_allocator, W_SEED_CST_BLOCK, 0);
  CHECK(named_plan != W_SEED_CST_NONE);
  CHECK(named_body != W_SEED_CST_NONE);
  CHECK(node_span_text(&named, named_plan,
                       ".fixed<capacity:64<iec.KiB>>"));
  CHECK(count_direct_kind(&named, named_body, W_SEED_CST_LET_STATEMENT) == 1);
  bool saw_allocator_argument = false;
  bool saw_ref_argument = false;
  for (size_t index = 0; index < named.result.node_count; index += 1) {
    if (named.nodes[index].kind != W_SEED_CST_ARGUMENT) continue;
    saw_allocator_argument |=
        node_span_text(&named, (w_seed_cst_index)index, "allocator:ref memory");
    saw_ref_argument |=
        node_span_text(&named, (w_seed_cst_index)index, "ref title");
  }
  CHECK(saw_allocator_argument);
  CHECK(saw_ref_argument);

  static const char nested_text[] =
      "fn nested(){allocator outer:.fixed<capacity:64<iec.KiB>>{"
      "allocator inner:.fixed<capacity:64<iec.KiB>>{let local=Array<String>()}"
      "let portable=Array<String>(allocator:outer)}}\n";
  fixture nested;
  CHECK(fixture_init(&nested, nested_text,
                     sizeof(nested.nodes) / sizeof(nested.nodes[0]),
                     sizeof(nested.issues) / sizeof(nested.issues[0])));
  CHECK(check_complete_shape(&nested));
  CHECK(count_kind(&nested, W_SEED_CST_ALLOCATOR_BLOCK) == 2);
  const w_seed_cst_index outer =
      first_kind(&nested, W_SEED_CST_ALLOCATOR_BLOCK);
  CHECK(outer != W_SEED_CST_NONE);
  const w_seed_cst_index outer_body =
      direct_child_after(&nested, outer, W_SEED_CST_BLOCK, 0);
  CHECK(outer_body != W_SEED_CST_NONE);
  const w_seed_cst_index inner = direct_child_after(
      &nested, outer_body, W_SEED_CST_ALLOCATOR_BLOCK, 0);
  CHECK(inner != W_SEED_CST_NONE);
  CHECK(nested.nodes[outer].raw_span.start_byte <
        nested.nodes[inner].raw_span.start_byte);
  CHECK(has_direct_text(&nested, outer, W_SEED_CST_WORD, "outer"));
  CHECK(has_direct_text(&nested, inner, W_SEED_CST_WORD, "inner"));
  bool saw_override = false;
  for (size_t index = 0; index < nested.result.node_count; index += 1) {
    if (nested.nodes[index].kind == W_SEED_CST_ARGUMENT &&
        node_span_text(&nested, (w_seed_cst_index)index, "allocator:outer")) {
      saw_override = true;
    }
  }
  CHECK(saw_override);

  static const struct {
    const char *text;
    w_seed_parse_issue_kind first_issue;
    bool missing_plan;
  } recovery_cases[] = {
      {"fn f(){allocator {let value=1}}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN, true},
      {"fn f(){allocator .fixed<capacity:64<iec.KiB>> let value=1}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE, false},
      {"fn f(){allocator .fixed<capacity:64<iec.KiB>>{let value=1}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE, false},
      {"fn f(){allocator 0:.fixed<capacity:64<iec.KiB>>{let value=1}}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN, false},
      {"fn f(){allocator .fixed<capacity:64<iec.KiB>>:{let value=1}}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN, false},
      {"fn f(){allocator .fixed<capacity:64<iec.KiB>{let value=1}}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE, false},
  };
  for (size_t index = 0;
       index < sizeof(recovery_cases) / sizeof(recovery_cases[0]); index += 1) {
    fixture recovery;
    CHECK(fixture_init(&recovery, recovery_cases[index].text,
                       sizeof(recovery.nodes) / sizeof(recovery.nodes[0]),
                       sizeof(recovery.issues) / sizeof(recovery.issues[0])));
    CHECK(recovery.result.status == W_SEED_PARSE_RECOVERED);
    CHECK(recovery.result.issue_count >= 1);
    CHECK(recovery.issues[0].kind == recovery_cases[index].first_issue);
    CHECK(count_kind(&recovery, W_SEED_CST_ALLOCATOR_BLOCK) == 1);
    const w_seed_cst_index allocator =
        first_kind(&recovery, W_SEED_CST_ALLOCATOR_BLOCK);
    CHECK(allocator != W_SEED_CST_NONE);
    CHECK(recovery.nodes[allocator].raw_span.start_byte <=
          recovery.nodes[allocator].raw_span.end_byte);
    if (recovery_cases[index].missing_plan) {
      CHECK(count_direct_kind(&recovery, allocator, W_SEED_CST_EXPRESSION) == 0);
      const w_seed_cst_index missing =
          direct_child_after(&recovery, allocator, W_SEED_CST_MISSING, 0);
      CHECK(missing != W_SEED_CST_NONE);
      CHECK(recovery.nodes[missing].raw_span.start_byte ==
            recovery.nodes[missing].raw_span.end_byte);
      CHECK(recovery.nodes[missing].raw_span.start_byte >=
            recovery.nodes[allocator].raw_span.start_byte);
      CHECK(recovery.nodes[missing].raw_span.start_byte <=
            recovery.nodes[allocator].raw_span.end_byte);
    }
    CHECK(check_leaf_partition(&recovery));
    CHECK(check_tree_links(&recovery));
  }

  static const char semicolon_text[] =
      "fn f(){allocator .fixed<capacity:64<iec.KiB>>{"
      "let snapshot=stage(ref title)};let after=next()}\n";
  fixture semicolon;
  CHECK(fixture_init(&semicolon, semicolon_text,
                     sizeof(semicolon.nodes) / sizeof(semicolon.nodes[0]),
                     sizeof(semicolon.issues) / sizeof(semicolon.issues[0])));
  CHECK(check_complete_shape(&semicolon));
  const w_seed_cst_index semicolon_allocator =
      first_kind(&semicolon, W_SEED_CST_ALLOCATOR_BLOCK);
  CHECK(semicolon_allocator != W_SEED_CST_NONE);
  const w_seed_cst_index semicolon_plan = direct_child_after(
      &semicolon, semicolon_allocator, W_SEED_CST_EXPRESSION, 0);
  const w_seed_cst_index semicolon_body = direct_child_after(
      &semicolon, semicolon_allocator, W_SEED_CST_BLOCK, 0);
  CHECK(semicolon_plan != W_SEED_CST_NONE);
  CHECK(semicolon_body != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&semicolon, semicolon_allocator,
                          W_SEED_CST_PUNCTUATION) >= 1);
  CHECK(has_direct_text(&semicolon, semicolon_allocator,
                        W_SEED_CST_PUNCTUATION, ";"));
  CHECK(semicolon.nodes[semicolon_plan].raw_span.end_byte <=
        semicolon.nodes[semicolon_body].raw_span.start_byte);
  CHECK(semicolon.nodes[semicolon_body].raw_span.end_byte <=
        semicolon.nodes[semicolon_allocator].raw_span.end_byte);
  const w_seed_cst_index function_body = first_kind(&semicolon, W_SEED_CST_BLOCK);
  CHECK(function_body != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&semicolon, function_body, W_SEED_CST_LET_STATEMENT) == 1);
  const w_seed_cst_index after =
      direct_child_after(&semicolon, function_body, W_SEED_CST_LET_STATEMENT, 0);
  CHECK(after != W_SEED_CST_NONE);
  CHECK(node_span_text(&semicolon, after, "let after=next()"));
  fixture semicolon_repeat;
  CHECK(fixture_init(&semicolon_repeat, semicolon_text,
                     sizeof(semicolon_repeat.nodes) /
                         sizeof(semicolon_repeat.nodes[0]),
                     sizeof(semicolon_repeat.issues) /
                         sizeof(semicolon_repeat.issues[0])));
  CHECK(same_parse(&semicolon, &semicolon_repeat));

  static const char *const fatal_texts[] = {
      "fn f(){try allocator .fixed<capacity:64<iec.KiB>>{let value=1}}\n",
      "fn f(){return try allocator .fixed<capacity:64<iec.KiB>>{}}\n",
      "allocator .fixed<capacity:64<iec.KiB>>{}\n",
      "fn f(){spawn<.compute> let value=work()}\n",
  };
  for (size_t index = 0;
       index < sizeof(fatal_texts) / sizeof(fatal_texts[0]); index += 1) {
    fixture fatal;
    CHECK(fixture_init(&fatal, fatal_texts[index],
                       sizeof(fatal.nodes) / sizeof(fatal.nodes[0]),
                       sizeof(fatal.issues) / sizeof(fatal.issues[0])));
    CHECK(fatal.result.status == W_SEED_PARSE_FATAL);
    CHECK(fatal.result.issue_count == 1);
    CHECK(fatal.issues[0].kind == W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
    CHECK(check_leaf_partition(&fatal));
    CHECK(check_tree_links(&fatal));
  }
  fixture foreign;
  CHECK(fixture_init(&foreign, "fn f(){foreign c { host body }}\n",
                     sizeof(foreign.nodes) / sizeof(foreign.nodes[0]),
                     sizeof(foreign.issues) / sizeof(foreign.issues[0])));
  CHECK(foreign.result.status == W_SEED_PARSE_FATAL);
  CHECK(foreign.result.issue_count == 1);
  CHECK(foreign.issues[0].kind == W_SEED_PARSE_ISSUE_FOREIGN_UNSUPPORTED);
  CHECK(check_leaf_partition(&foreign));
  CHECK(check_tree_links(&foreign));
  return true;
}

static bool test_phase2_generic_contract_switch(void) {
  static const char generic_text[] =
      "struct Box<_ state:State>{value:state}\n"
      "fn identity<T:Order>(value:T):T{return value}\n"
      "type Alias<T:Order> = Array<T>\n"
      "alias Legacy<U> = Array<Array<u8>>\n";
  fixture generic;
  CHECK(fixture_init(&generic, generic_text,
                     sizeof(generic.nodes) / sizeof(generic.nodes[0]),
                     sizeof(generic.issues) / sizeof(generic.issues[0])));
  CHECK(check_complete_shape(&generic));
  CHECK(count_kind(&generic, W_SEED_CST_GENERIC_PARAMETERS) == 4);
  CHECK(count_kind(&generic, W_SEED_CST_GENERIC_PARAMETER) == 4);
  const w_seed_cst_index structure = first_kind(&generic, W_SEED_CST_STRUCT);
  const w_seed_cst_index function = first_kind(&generic, W_SEED_CST_FUNCTION);
  const w_seed_cst_index type_decl = first_kind(&generic,
                                                W_SEED_CST_TYPE_DECLARATION);
  const w_seed_cst_index alias_decl = first_kind(&generic,
                                                 W_SEED_CST_ALIAS_DECLARATION);
  CHECK(structure != W_SEED_CST_NONE);
  CHECK(function != W_SEED_CST_NONE);
  CHECK(type_decl != W_SEED_CST_NONE);
  CHECK(alias_decl != W_SEED_CST_NONE);
  const w_seed_cst_index structure_generics = direct_child_after(
      &generic, structure, W_SEED_CST_GENERIC_PARAMETERS, 0);
  const w_seed_cst_index function_generics = direct_child_after(
      &generic, function, W_SEED_CST_GENERIC_PARAMETERS, 0);
  const w_seed_cst_index type_generics = direct_child_after(
      &generic, type_decl, W_SEED_CST_GENERIC_PARAMETERS, 0);
  const w_seed_cst_index alias_generics = direct_child_after(
      &generic, alias_decl, W_SEED_CST_GENERIC_PARAMETERS, 0);
  CHECK(structure_generics != W_SEED_CST_NONE);
  CHECK(function_generics != W_SEED_CST_NONE);
  CHECK(type_generics != W_SEED_CST_NONE);
  CHECK(alias_generics != W_SEED_CST_NONE);
  const w_seed_cst_index structure_parameter = direct_child_after(
      &generic, structure_generics, W_SEED_CST_GENERIC_PARAMETER, 0);
  CHECK(structure_parameter != W_SEED_CST_NONE);
  CHECK(has_direct_text(&generic, structure_parameter, W_SEED_CST_WORD, "_"));
  CHECK(has_direct_text(&generic, structure_parameter, W_SEED_CST_WORD,
                        "state"));
  CHECK(has_direct_text(&generic, structure_parameter,
                        W_SEED_CST_PUNCTUATION, ":"));
  CHECK(direct_child_after(&generic, structure_parameter, W_SEED_CST_TYPE, 0) !=
        W_SEED_CST_NONE);
  CHECK(generic.nodes[structure_generics].raw_span.end_byte <=
        generic.nodes[structure].raw_span.end_byte);
  CHECK(generic.nodes[function_generics].raw_span.end_byte <=
        generic.nodes[function].raw_span.end_byte);
  const w_seed_cst_index type_value =
      direct_child_after(&generic, type_decl, W_SEED_CST_TYPE, 0);
  const w_seed_cst_index alias_value =
      direct_child_after(&generic, alias_decl, W_SEED_CST_TYPE, 0);
  CHECK(type_value != W_SEED_CST_NONE);
  CHECK(alias_value != W_SEED_CST_NONE);
  CHECK(generic.nodes[type_generics].raw_span.end_byte <=
        generic.nodes[type_value].raw_span.start_byte);
  CHECK(generic.nodes[alias_generics].raw_span.end_byte <=
        generic.nodes[alias_value].raw_span.start_byte);
  CHECK(count_direct_kind(&generic, alias_value, W_SEED_CST_CONTRACT_ENVELOPE) ==
        1);
  size_t raw_shift_count = 0;
  for (size_t index = 0; index < generic.result.node_count; index += 1) {
    if (generic.nodes[index].kind != W_SEED_CST_PUNCTUATION) continue;
    if (node_span_text(&generic, (w_seed_cst_index)index, ">>")) {
      raw_shift_count += 1;
    }
  }
  CHECK(raw_shift_count == 1);
  fixture generic_repeat;
  CHECK(fixture_init(&generic_repeat, generic_text,
                     sizeof(generic_repeat.nodes) /
                         sizeof(generic_repeat.nodes[0]),
                     sizeof(generic_repeat.issues) /
                         sizeof(generic_repeat.issues[0])));
  CHECK(same_parse(&generic, &generic_repeat));

  static const char optional_text[] =
      "struct OvenSession<_ state:OvenSessionState>{}\n"
      "fn open(_ name:String){}\n"
      "fn demo(){let positional=OvenSession<.ready>.state;"
      "let named=OvenSession<state:.ready>.state;open(\"oven\");"
      "open(name:\"oven\")}\n";
  fixture optional;
  CHECK(fixture_init(&optional, optional_text,
                     sizeof(optional.nodes) / sizeof(optional.nodes[0]),
                     sizeof(optional.issues) / sizeof(optional.issues[0])));
  CHECK(check_complete_shape(&optional));
  CHECK(count_kind(&optional, W_SEED_CST_GENERIC_PARAMETERS) == 1);
  CHECK(count_kind(&optional, W_SEED_CST_GENERIC_PARAMETER) == 1);
  CHECK(count_kind(&optional, W_SEED_CST_CONTRACT_ENVELOPE) == 2);
  size_t positional_envelopes = 0;
  size_t named_envelopes = 0;
  for (size_t index = 0; index < optional.result.node_count; index += 1) {
    if (optional.nodes[index].kind != W_SEED_CST_CONTRACT_ENVELOPE) continue;
    const w_seed_cst_index envelope = (w_seed_cst_index)index;
    CHECK(count_direct_kind(&optional, envelope, W_SEED_CST_PUNCTUATION) >= 2);
    CHECK(has_direct_text(&optional, envelope, W_SEED_CST_WORD, "ready"));
    if (has_direct_text(&optional, envelope, W_SEED_CST_WORD, "state")) {
      named_envelopes += 1;
      CHECK(has_direct_text(&optional, envelope, W_SEED_CST_PUNCTUATION, ":"));
    } else {
      positional_envelopes += 1;
    }
  }
  CHECK(positional_envelopes == 1);
  CHECK(named_envelopes == 1);
  const w_seed_cst_index demo = first_kind(&optional, W_SEED_CST_FUNCTION);
  CHECK(demo != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&optional, demo, W_SEED_CST_BLOCK) == 1);
  fixture optional_repeat;
  CHECK(fixture_init(&optional_repeat, optional_text,
                     sizeof(optional_repeat.nodes) /
                         sizeof(optional_repeat.nodes[0]),
                     sizeof(optional_repeat.issues) /
                         sizeof(optional_repeat.issues[0])));
  CHECK(same_parse(&optional, &optional_repeat));

  static const char contract_text[] =
      "import {Course,Order} from domain;\n"
      "type Active=Array<Order><(.count<=64)>;\n"
      "type Later=Course<[.horizonCake,.nebulaBroth]>;\n";
  fixture contracts;
  CHECK(fixture_init(&contracts, contract_text,
                     sizeof(contracts.nodes) / sizeof(contracts.nodes[0]),
                     sizeof(contracts.issues) / sizeof(contracts.issues[0])));
  CHECK(check_complete_shape(&contracts));
  CHECK(count_kind(&contracts, W_SEED_CST_TYPE_DECLARATION) == 2);
  CHECK(count_kind(&contracts, W_SEED_CST_CONTRACT_ENVELOPE) == 3);
  const w_seed_cst_index active = first_kind(&contracts,
                                             W_SEED_CST_TYPE_DECLARATION);
  w_seed_cst_index later = W_SEED_CST_NONE;
  for (size_t index = (size_t)active + 1; index < contracts.result.node_count;
       index += 1) {
    if (contracts.nodes[index].kind == W_SEED_CST_TYPE_DECLARATION) {
      later = (w_seed_cst_index)index;
      break;
    }
  }
  CHECK(later != W_SEED_CST_NONE);
  const w_seed_cst_index active_type =
      direct_child_after(&contracts, active, W_SEED_CST_TYPE, 0);
  const w_seed_cst_index later_type =
      direct_child_after(&contracts, later, W_SEED_CST_TYPE, 0);
  CHECK(active_type != W_SEED_CST_NONE);
  CHECK(later_type != W_SEED_CST_NONE);
  CHECK(node_span_text(&contracts, active_type,
                       "Array<Order><(.count<=64)>"));
  CHECK(node_span_text(&contracts, later_type,
                       "Course<[.horizonCake,.nebulaBroth]>"));
  CHECK(count_direct_kind(&contracts, active_type,
                          W_SEED_CST_CONTRACT_ENVELOPE) == 2);
  CHECK(count_direct_kind(&contracts, later_type,
                          W_SEED_CST_CONTRACT_ENVELOPE) == 1);
  const w_seed_cst_index predicate = direct_child_after(
      &contracts, active_type, W_SEED_CST_CONTRACT_ENVELOPE, 1);
  const w_seed_cst_index list = direct_child_after(
      &contracts, later_type, W_SEED_CST_CONTRACT_ENVELOPE, 0);
  CHECK(predicate != W_SEED_CST_NONE);
  CHECK(list != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&contracts, predicate, W_SEED_CST_EXPRESSION) == 1);
  CHECK(count_direct_kind(&contracts, list, W_SEED_CST_ARRAY) == 1);
  fixture contracts_repeat;
  CHECK(fixture_init(&contracts_repeat, contract_text,
                     sizeof(contracts_repeat.nodes) /
                         sizeof(contracts_repeat.nodes[0]),
                     sizeof(contracts_repeat.issues) /
                         sizeof(contracts_repeat.issues[0])));
  CHECK(same_parse(&contracts, &contracts_repeat));

  static const char enum_text[] =
      "alias WorkStage=ServiceStage<[.reserving,.preparing,.serving]>;"
      "fn instruction(stage:WorkStage):String{return switch stage{"
      "case .reserving:\"Reserve\" case .preparing:\"Prepare\" "
      "case .serving:\"Serve\"}}\n";
  fixture enumeration;
  CHECK(fixture_init(&enumeration, enum_text,
                     sizeof(enumeration.nodes) / sizeof(enumeration.nodes[0]),
                     sizeof(enumeration.issues) / sizeof(enumeration.issues[0])));
  CHECK(check_complete_shape(&enumeration));
  CHECK(count_kind(&enumeration, W_SEED_CST_ALIAS_DECLARATION) == 1);
  CHECK(count_kind(&enumeration, W_SEED_CST_SWITCH_EXPRESSION) == 1);
  CHECK(count_kind(&enumeration, W_SEED_CST_SWITCH_ARM) == 3);
  CHECK(count_kind(&enumeration, W_SEED_CST_ARRAY) == 1);
  const w_seed_cst_index switch_node =
      first_kind(&enumeration, W_SEED_CST_SWITCH_EXPRESSION);
  CHECK(switch_node != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&enumeration, switch_node, W_SEED_CST_SWITCH_ARM) ==
        3);
  const char *const arm_texts[] = {"case .reserving:\"Reserve\" ",
                                   "case .preparing:\"Prepare\" ",
                                   "case .serving:\"Serve\""};
  for (size_t index = 0; index < 3; index += 1) {
    const w_seed_cst_index arm = direct_child_after(
        &enumeration, switch_node, W_SEED_CST_SWITCH_ARM, index);
    CHECK(arm != W_SEED_CST_NONE);
    CHECK(node_span_text(&enumeration, arm, arm_texts[index]));
  }
  fixture enum_repeat;
  CHECK(fixture_init(&enum_repeat, enum_text,
                     sizeof(enum_repeat.nodes) / sizeof(enum_repeat.nodes[0]),
                     sizeof(enum_repeat.issues) / sizeof(enum_repeat.issues[0])));
  CHECK(same_parse(&enumeration, &enum_repeat));

  static const struct {
    const char *text;
    w_seed_parse_status status;
    w_seed_parse_issue_kind issue;
  } mutations[] = {
      {"fn id <T>(value:T):T{return value}\n", W_SEED_PARSE_RECOVERED,
       W_SEED_PARSE_ISSUE_SPACED_HEAD},
      {"struct Box <T:Order>{}\n", W_SEED_PARSE_RECOVERED,
       W_SEED_PARSE_ISSUE_SPACED_HEAD},
      {"fn id<:T>(value:T):T{return value}\n", W_SEED_PARSE_RECOVERED,
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn id<T Order>(value:T):T{return value}\n", W_SEED_PARSE_RECOVERED,
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn id<T:>(value:T):T{return value}\n", W_SEED_PARSE_RECOVERED,
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn id<T(value:T):T{return value}\n", W_SEED_PARSE_RECOVERED,
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"type A=Course<[.ready,,.later]>\n", W_SEED_PARSE_RECOVERED,
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"type A=Course<(.count<=1;>\n", W_SEED_PARSE_RECOVERED,
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"type A=Course<state:>\n", W_SEED_PARSE_RECOVERED,
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(x:X):String{return switch x{}}\n", W_SEED_PARSE_RECOVERED,
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(x:X):String{return switch x{case .a \"A\"}}\n",
       W_SEED_PARSE_RECOVERED, W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn f(x:X):String{return switch x{case .a:\"A\"}\n",
       W_SEED_PARSE_RECOVERED, W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
  };
  for (size_t index = 0; index < sizeof(mutations) / sizeof(mutations[0]);
       index += 1) {
    fixture malformed;
    CHECK(fixture_init(&malformed, mutations[index].text,
                       sizeof(malformed.nodes) / sizeof(malformed.nodes[0]),
                       sizeof(malformed.issues) / sizeof(malformed.issues[0])));
    CHECK(malformed.result.status == mutations[index].status);
    CHECK(malformed.result.issue_count >= 1);
    CHECK(has_issue(&malformed, mutations[index].issue));
    CHECK(check_leaf_partition(&malformed));
    CHECK(check_tree_links(&malformed));
  }
  fixture comparison;
  CHECK(fixture_init(&comparison,
                     "fn f(left:Bool,right:Bool):Bool{return left < right}\n",
                     sizeof(comparison.nodes) / sizeof(comparison.nodes[0]),
                     sizeof(comparison.issues) / sizeof(comparison.issues[0])));
  CHECK(check_complete_shape(&comparison));
  CHECK(count_kind(&comparison, W_SEED_CST_CONTRACT_ENVELOPE) == 0);
  fixture import_after;
  CHECK(fixture_init(&import_after,
                     "type A=Array<u8>\nimport {x} from module.path\n",
                     sizeof(import_after.nodes) / sizeof(import_after.nodes[0]),
                     sizeof(import_after.issues) / sizeof(import_after.issues[0])));
  CHECK(import_after.result.status == W_SEED_PARSE_FATAL);
  CHECK(import_after.result.issue_count == 1);
  CHECK(import_after.issues[0].kind == W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
  CHECK(check_leaf_partition(&import_after));
  CHECK(check_tree_links(&import_after));
  fixture import_after_alias;
  CHECK(fixture_init(&import_after_alias,
                     "alias A=Array<u8>\nimport {x} from module.path\n",
                     sizeof(import_after_alias.nodes) /
                         sizeof(import_after_alias.nodes[0]),
                     sizeof(import_after_alias.issues) /
                         sizeof(import_after_alias.issues[0])));
  CHECK(import_after_alias.result.status == W_SEED_PARSE_FATAL);
  CHECK(import_after_alias.result.issue_count == 1);
  CHECK(import_after_alias.issues[0].kind == W_SEED_PARSE_ISSUE_UNSUPPORTED_FORM);
  CHECK(check_leaf_partition(&import_after_alias));
  CHECK(check_tree_links(&import_after_alias));

  static const char semicolon_text[] =
      "type A=Array<u8>;alias B=Array<A>;fn f<T>(x:T):T{return x}\n";
  fixture semicolon;
  CHECK(fixture_init(&semicolon, semicolon_text,
                     sizeof(semicolon.nodes) / sizeof(semicolon.nodes[0]),
                     sizeof(semicolon.issues) / sizeof(semicolon.issues[0])));
  CHECK(check_complete_shape(&semicolon));
  static const char semicolonless_text[] =
      "type A=Array<u8>\nalias B=Array<A>\nfn f<T>(x:T):T{return x}\n";
  fixture semicolonless;
  CHECK(fixture_init(&semicolonless, semicolonless_text,
                     sizeof(semicolonless.nodes) /
                         sizeof(semicolonless.nodes[0]),
                     sizeof(semicolonless.issues) /
                         sizeof(semicolonless.issues[0])));
  CHECK(check_complete_shape(&semicolonless));
  return true;
}

static bool has_issue(const fixture *fixture_value,
                      w_seed_parse_issue_kind kind) {
  for (size_t index = 0; index < fixture_value->result.issue_count; index += 1) {
    if (fixture_value->issues[index].kind == kind) return true;
  }
  return false;
}

static bool test_repeat_array_shape(void) {
  static const char text[] =
      "fn zeros():Array<Int>{return [0;16]}\n";
  fixture value;
  CHECK(fixture_init(&value, text,
                     sizeof(value.nodes) / sizeof(value.nodes[0]),
                     sizeof(value.issues) / sizeof(value.issues[0])));
  CHECK(value.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(value.result.issue_count == 0);
  CHECK(check_leaf_partition(&value));
  CHECK(check_tree_links(&value));

  const w_seed_cst_index array = first_kind(&value, W_SEED_CST_ARRAY);
  CHECK(array != W_SEED_CST_NONE);
  CHECK(count_direct_kind(&value, array, W_SEED_CST_EXPRESSION) == 2);
  CHECK(node_span_text(&value,
                       direct_child_after(&value, array, W_SEED_CST_EXPRESSION,
                                          0),
                       "0"));
  CHECK(node_span_text(&value,
                       direct_child_after(&value, array, W_SEED_CST_EXPRESSION,
                                          1),
                       "16"));
  CHECK(has_direct_text(&value, array, W_SEED_CST_PUNCTUATION, ";"));

  fixture repeat;
  CHECK(fixture_init(&repeat, text,
                     sizeof(repeat.nodes) / sizeof(repeat.nodes[0]),
                     sizeof(repeat.issues) / sizeof(repeat.issues[0])));
  CHECK(memcmp(&value.result, &repeat.result, sizeof(value.result)) == 0);
  CHECK(memcmp(value.nodes, repeat.nodes,
               value.result.node_count * sizeof(value.nodes[0])) == 0);
  CHECK(memcmp(value.issues, repeat.issues,
               value.result.issue_count * sizeof(value.issues[0])) == 0);

  fixture nested;
  CHECK(fixture_init(&nested,
                     "fn nested():Array<Array<Int>>{return [[0;16];2]}\n",
                     sizeof(nested.nodes) / sizeof(nested.nodes[0]),
                     sizeof(nested.issues) / sizeof(nested.issues[0])));
  CHECK(nested.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(check_leaf_partition(&nested));
  CHECK(check_tree_links(&nested));
  size_t repeat_arrays = 0;
  for (size_t index = 0; index < nested.result.node_count; index += 1) {
    if (nested.nodes[index].kind != W_SEED_CST_ARRAY) continue;
    CHECK(count_direct_kind(&nested, (w_seed_cst_index)index,
                            W_SEED_CST_EXPRESSION) == 2);
    repeat_arrays += 1;
  }
  CHECK(repeat_arrays == 2);

  fixture adjacent;
  CHECK(fixture_init(&adjacent,
                     "fn adjacent(){let values=[0;2];return values}\n",
                     sizeof(adjacent.nodes) / sizeof(adjacent.nodes[0]),
                     sizeof(adjacent.issues) / sizeof(adjacent.issues[0])));
  CHECK(adjacent.result.status == W_SEED_PARSE_COMPLETE);
  CHECK(check_leaf_partition(&adjacent));
  CHECK(check_tree_links(&adjacent));
  const w_seed_cst_index adjacent_array =
      first_kind(&adjacent, W_SEED_CST_ARRAY);
  const w_seed_cst_index let_statement =
      first_kind(&adjacent, W_SEED_CST_LET_STATEMENT);
  CHECK(has_direct_text(&adjacent, adjacent_array, W_SEED_CST_PUNCTUATION,
                        ";"));
  CHECK(has_direct_text(&adjacent, let_statement, W_SEED_CST_PUNCTUATION,
                        ";"));
  return true;
}

static bool test_repeat_array_recovery(void) {
  static const struct {
    const char *text;
    w_seed_parse_issue_kind issue;
  } mutations[] = {
      {"fn zeros():Array<Int>{return [0 16]}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
      {"fn zeros():Array<Int>{return [0;]}\n",
       W_SEED_PARSE_ISSUE_UNEXPECTED_TOKEN},
      {"fn zeros():Array<Int>{return [0;16}\n",
       W_SEED_PARSE_ISSUE_MISSING_OWNER_CLOSE},
  };
  for (size_t index = 0; index < sizeof(mutations) / sizeof(mutations[0]);
       index += 1) {
    fixture value;
    CHECK(fixture_init(&value, mutations[index].text,
                       sizeof(value.nodes) / sizeof(value.nodes[0]),
                       sizeof(value.issues) / sizeof(value.issues[0])));
    CHECK(value.result.status == W_SEED_PARSE_RECOVERED);
    CHECK(has_issue(&value, mutations[index].issue));
    CHECK(check_leaf_partition(&value));
    CHECK(check_tree_links(&value));
  }
  return true;
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
  const bool passed =
      test_positive_core() &&
      test_nested_close_and_shift() &&
      test_pratt_nesting() &&
      test_pratt_operator_table() &&
      test_repeat_array_shape() &&
      test_repeat_array_recovery() &&
      test_subspan_bounds() &&
      test_adjacency_and_boundaries() &&
      test_recovery_codes() &&
      test_fail_closed() &&
      test_capacity() &&
      test_init_validation() &&
      test_parse_twice() &&
      test_phase2_declaration_tree() &&
      test_borrow_clause_shapes() &&
      test_async_function_shapes() &&
      test_transaction_shapes() &&
      test_language_lock_shapes() &&
      test_phase2_parameter_and_argument_shapes() &&
      test_phase2_parameter_requirements() &&
      test_phase2_prefix_forms() &&
      test_phase2_fatal_boundaries() &&
      test_phase2_recovery_mutations() &&
      test_phase2_generic_contract_switch() &&
      test_allocator_block_shapes() &&
      test_for_control_shapes() &&
      test_for_markers_and_iterables() &&
      test_for_control_recovery();
  if (!passed) return 1;
  (void)puts("Seed C parser: caller-owned CST, recovery and incremental hand cases passed");
  return 0;
}
