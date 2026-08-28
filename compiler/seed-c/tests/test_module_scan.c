#include "w_seed_module_scan.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "module scan check failed: %s (%s:%d)\n",         \
                    #condition, __FILE__, __LINE__);                           \
      return false;                                                             \
    }                                                                           \
  } while (0)

enum {
  TEST_LEX_FRAMES = 128,
  TEST_TOKENS = 256,
  TEST_NODES = 2048,
  TEST_PARSE_FRAMES = 256,
  TEST_ISSUES = 64,
  TEST_ORIGINS = 16,
};

typedef struct {
  w_seed_source source;
  w_seed_lexer_frame lexer_frames[TEST_LEX_FRAMES];
  w_seed_parse_token tokens[TEST_TOKENS];
  w_seed_cst_node nodes[TEST_NODES];
  w_seed_parse_frame parse_frames[TEST_PARSE_FRAMES];
  w_seed_parse_issue issues[TEST_ISSUES];
  w_seed_parser parser;
  w_seed_parse_result parse;
} fixture;

static bool parse_text(fixture *value, const char *text) {
  const w_seed_byte_view bytes = {(const uint8_t *)text, strlen(text)};
  w_seed_source_error source_error;
  if (!w_seed_source_init(bytes, &value->source, &source_error)) return false;
  w_seed_lex_error lex_error;
  if (!w_seed_parser_init(
          &value->source, (w_seed_span){0u, bytes.length},
          (w_seed_foreign_limits){65536u, 256u}, value->lexer_frames,
          TEST_LEX_FRAMES, value->tokens, TEST_TOKENS, value->nodes,
          TEST_NODES, value->parse_frames, TEST_PARSE_FRAMES, value->issues,
          TEST_ISSUES, &value->parser, &lex_error)) {
    return false;
  }
  return w_seed_parser_parse(&value->parser, &value->parse);
}

static bool all_bytes_equal(const uint8_t *bytes, size_t length,
                            uint8_t value) {
  for (size_t index = 0u; index < length; index += 1u) {
    if (bytes[index] != value) return false;
  }
  return true;
}

static bool test_forms_and_spans(void) {
  static const char source[] =
      "module kitchen\n"
      "import dep;\n"
      "import dep.path;\n"
      "import alias from package.menu;\n"
      "import * from wildcard.path\n"
      "import {value,other} from kitchen.menu\n";
  fixture value;
  CHECK(parse_text(&value, source));
  CHECK(value.parse.status == W_SEED_PARSE_COMPLETE &&
        value.parse.issue_count == 0u);
  w_seed_module_origin origins[TEST_ORIGINS];
  w_seed_module_scan_result result;
  CHECK(w_seed_module_scan(&value.source, value.nodes, value.parse.node_count,
                           &value.parse, NULL, 0u, &result) ==
        W_SEED_MODULE_SCAN_CAPACITY);
  CHECK(result.required == 5u && result.written == 0u &&
        result.has_module_header_name &&
        result.module_header_name_span.start_byte == 7u &&
        result.module_header_name_span.end_byte == 14u);
  CHECK(w_seed_module_scan(&value.source, value.nodes, value.parse.node_count,
                           &value.parse, origins, 5u, &result) ==
        W_SEED_MODULE_SCAN_OK);
  CHECK(result.required == 5u && result.written == 5u);
  static const char *const paths[] = {
      "dep", "dep.path", "package.menu", "wildcard.path", "kitchen.menu"};
  for (size_t index = 0u; index < 5u; index += 1u) {
    CHECK(origins[index].kind == W_SEED_MODULE_ORIGIN_IMPORT);
    CHECK(origins[index].direct_import_ordinal == (uint32_t)index);
    CHECK(origins[index].cst_node_index < value.parse.node_count);
    CHECK(origins[index].declaration_span.start_byte <
          origins[index].declaration_span.end_byte);
    const size_t path_length = strlen(paths[index]);
    CHECK(origins[index].module_path_span.end_byte -
              origins[index].module_path_span.start_byte ==
          path_length);
    CHECK(memcmp(value.source.bytes.data +
                     origins[index].module_path_span.start_byte,
                 paths[index], path_length) == 0);
    CHECK(w_seed_source_validate_span(&value.source,
                                      origins[index].declaration_span, NULL));
    CHECK(w_seed_source_validate_span(&value.source,
                                      origins[index].module_path_span, NULL));
  }
  return true;
}

static bool test_capacity_and_all_or_nothing(void) {
  static const char source[] = "import one;\nimport two;\n";
  fixture value;
  CHECK(parse_text(&value, source));
  w_seed_module_origin origins[2];
  (void)memset(origins, 0xa5, sizeof(origins));
  w_seed_module_scan_result result;
  CHECK(w_seed_module_scan(&value.source, value.nodes, value.parse.node_count,
                           &value.parse, origins, 1u, &result) ==
        W_SEED_MODULE_SCAN_CAPACITY);
  CHECK(result.required == 2u && result.written == 0u);
  CHECK(((const uint8_t *)origins)[0] == 0xa5u &&
        ((const uint8_t *)origins)[sizeof(origins) - 1u] == 0xa5u);
  CHECK(w_seed_module_scan(&value.source, value.nodes, value.parse.node_count,
                           &value.parse, origins, 2u, &result) ==
        W_SEED_MODULE_SCAN_OK);
  CHECK(result.written == 2u);
  w_seed_module_origin repeat[2] = {{0}};
  CHECK(w_seed_module_scan(&value.source, value.nodes, value.parse.node_count,
                           &value.parse, repeat, 2u, &result) ==
        W_SEED_MODULE_SCAN_OK);
  for (size_t index = 0u; index < 2u; index += 1u) {
    CHECK(origins[index].kind == repeat[index].kind &&
          origins[index].direct_import_ordinal ==
              repeat[index].direct_import_ordinal &&
          origins[index].cst_node_index == repeat[index].cst_node_index &&
          origins[index].declaration_span.start_byte ==
              repeat[index].declaration_span.start_byte &&
          origins[index].declaration_span.end_byte ==
              repeat[index].declaration_span.end_byte &&
          origins[index].module_path_span.start_byte ==
              repeat[index].module_path_span.start_byte &&
          origins[index].module_path_span.end_byte ==
              repeat[index].module_path_span.end_byte);
  }
  return true;
}

static bool test_invalid_inputs(void) {
  static const char source[] = "import one\n";
  fixture value;
  CHECK(parse_text(&value, source));
  w_seed_module_origin origins[1];
  (void)memset(origins, 0xa5, sizeof(origins));
  w_seed_module_scan_result result;
  w_seed_parse_result incomplete = value.parse;
  incomplete.status = W_SEED_PARSE_RECOVERED;
  CHECK(w_seed_module_scan(&value.source, value.nodes, value.parse.node_count,
                           &incomplete, origins, 1u, &result) ==
        W_SEED_MODULE_SCAN_INVALID);
  CHECK(all_bytes_equal((const uint8_t *)origins, sizeof(origins), 0xa5u));

  w_seed_cst_node root_saved = value.nodes[value.parse.root];
  value.nodes[value.parse.root].raw_span.end_byte += 1u;
  (void)memset(origins, 0xa5, sizeof(origins));
  CHECK(w_seed_module_scan(&value.source, value.nodes, value.parse.node_count,
                           &value.parse, origins, 1u, &result) ==
        W_SEED_MODULE_SCAN_INVALID);
  CHECK(all_bytes_equal((const uint8_t *)origins, sizeof(origins), 0xa5u));
  value.nodes[value.parse.root] = root_saved;

  uint32_t import_node = W_SEED_CST_NONE;
  for (size_t index = 0u; index < value.parse.node_count; index += 1u) {
    if (value.nodes[index].kind == W_SEED_CST_IMPORT) {
      import_node = (uint32_t)index;
      break;
    }
  }
  CHECK(import_node != W_SEED_CST_NONE && import_node < value.parse.node_count);
  w_seed_cst_node saved = value.nodes[import_node];
  value.nodes[import_node].next_sibling = (uint32_t)value.parse.node_count;
  (void)memset(origins, 0xa5, sizeof(origins));
  CHECK(w_seed_module_scan(&value.source, value.nodes, value.parse.node_count,
                           &value.parse, origins, 1u, &result) ==
        W_SEED_MODULE_SCAN_INVALID);
  CHECK(all_bytes_equal((const uint8_t *)origins, sizeof(origins), 0xa5u));
  value.nodes[import_node] = saved;

  value.nodes[value.parse.root].kind = W_SEED_CST_IMPORT;
  (void)memset(origins, 0xa5, sizeof(origins));
  CHECK(w_seed_module_scan(&value.source, value.nodes, value.parse.node_count,
                           &value.parse, origins, 1u, &result) ==
        W_SEED_MODULE_SCAN_INVALID);
  value.nodes[value.parse.root] = root_saved;

  uint32_t import_token = W_SEED_CST_NONE;
  for (size_t index = 0u; index < value.parse.node_count; index += 1u) {
    if ((value.nodes[index].flags & W_SEED_CST_FLAG_RAW_LEAF) != 0u &&
        value.nodes[index].kind == W_SEED_CST_WORD &&
        value.nodes[index].raw_span.start_byte ==
            value.nodes[import_node].raw_span.start_byte &&
        value.nodes[index].raw_span.end_byte -
                value.nodes[index].raw_span.start_byte ==
            strlen("import") &&
        memcmp(value.source.bytes.data + value.nodes[index].raw_span.start_byte,
               "import", strlen("import")) == 0) {
      import_token = (uint32_t)index;
      break;
    }
  }
  CHECK(import_token != W_SEED_CST_NONE && import_token < value.parse.node_count);
  w_seed_cst_node malformed_token = value.nodes[import_token];
  value.nodes[import_token].kind = W_SEED_CST_NUMBER;
  (void)memset(origins, 0xa5, sizeof(origins));
  const w_seed_module_scan_status malformed_status =
      w_seed_module_scan(&value.source, value.nodes, value.parse.node_count,
                         &value.parse, origins, 1u, &result);
  CHECK(malformed_status == W_SEED_MODULE_SCAN_UNSUPPORTED);
  CHECK(all_bytes_equal((const uint8_t *)origins, sizeof(origins), 0xa5u));
  value.nodes[import_token] = malformed_token;

  static const char utf8_source[] = "import caf\xc3\xa9\n";
  fixture utf8;
  CHECK(parse_text(&utf8, utf8_source));
  CHECK(utf8.parse.status == W_SEED_PARSE_COMPLETE &&
        utf8.parse.issue_count == 0u);
  const uint32_t utf8_import = utf8.nodes[utf8.parse.root].first_child;
  CHECK(utf8_import != W_SEED_CST_NONE);
  saved = utf8.nodes[utf8_import];
  utf8.nodes[utf8_import].raw_span.start_byte = 11u;
  (void)memset(origins, 0xa5, sizeof(origins));
  CHECK(w_seed_module_scan(&utf8.source, utf8.nodes, utf8.parse.node_count,
                           &utf8.parse, origins, 1u, &result) ==
        W_SEED_MODULE_SCAN_INVALID);
  CHECK(all_bytes_equal((const uint8_t *)origins, sizeof(origins), 0xa5u));
  utf8.nodes[utf8_import] = saved;

  static const char ordered_source[] = "import one\nimport two\n";
  fixture ordered;
  CHECK(parse_text(&ordered, ordered_source));
  const uint32_t first_import = ordered.nodes[ordered.parse.root].first_child;
  CHECK(first_import != W_SEED_CST_NONE);
  const uint32_t second_import = ordered.nodes[first_import].next_sibling;
  CHECK(second_import != W_SEED_CST_NONE);
  saved = ordered.nodes[second_import];
  ordered.nodes[second_import].raw_span.start_byte =
      ordered.nodes[first_import].raw_span.start_byte;
  (void)memset(origins, 0xa5, sizeof(origins));
  CHECK(w_seed_module_scan(&ordered.source, ordered.nodes,
                           ordered.parse.node_count, &ordered.parse, origins,
                           2u, &result) == W_SEED_MODULE_SCAN_INVALID);
  CHECK(all_bytes_equal((const uint8_t *)origins, sizeof(origins), 0xa5u));
  ordered.nodes[second_import] = saved;

  w_seed_span path;
  CHECK(!w_seed_module_scan_import_path_span(
          &value.source, value.nodes, value.parse.node_count,
      (w_seed_span){1u, 2u}, &path));
  return true;
}

int main(void) {
  if (!test_forms_and_spans()) return 1;
  if (!test_capacity_and_all_or_nothing()) return 1;
  if (!test_invalid_inputs()) return 1;
  return 0;
}
