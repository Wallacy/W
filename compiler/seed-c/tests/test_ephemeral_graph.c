#include "w_seed_ephemeral_graph.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "ephemeral graph check failed: %s (%s:%d)\n",    \
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
  w_seed_frontend_document document;
} document_fixture;

typedef struct {
  w_seed_ephemeral_graph_scratch_node nodes[4];
  w_seed_ephemeral_graph_scratch_edge edges[4];
  size_t sorted_nodes[4];
  size_t node_ordinals[4];
  size_t sorted_edges[4];
  size_t sorted_resolved_edges[4];
  w_seed_module_origin origins[4];
  uint32_t indegree[4];
  uint32_t queue[4];
  uint32_t depths[4];
  w_seed_ephemeral_graph_scratch scratch;
} test_graph_scratch;

static void init_graph_scratch(test_graph_scratch *storage,
                               size_t node_capacity, size_t edge_capacity,
                               size_t origin_capacity) {
  storage->scratch = (w_seed_ephemeral_graph_scratch){
      storage->nodes,
      node_capacity,
      storage->edges,
      edge_capacity,
      storage->sorted_nodes,
      node_capacity,
      storage->node_ordinals,
      node_capacity,
      storage->sorted_edges,
      edge_capacity,
      storage->sorted_resolved_edges,
      edge_capacity,
      storage->origins,
      origin_capacity,
      storage->indegree,
      node_capacity,
      storage->queue,
      node_capacity,
      storage->depths,
      node_capacity,
  };
}

static bool parse_document(document_fixture *fixture, const char *source_text,
                           w_seed_frontend_text source_id,
                           w_seed_frontend_text module_id,
                           w_seed_frontend_text local_name) {
  const w_seed_byte_view bytes = {(const uint8_t *)source_text,
                                  strlen(source_text)};
  w_seed_source_error source_error;
  CHECK(w_seed_source_init(bytes, &fixture->source, &source_error));
  w_seed_lex_error lex_error;
  CHECK(w_seed_parser_init(
      &fixture->source, (w_seed_span){0u, bytes.length},
      (w_seed_foreign_limits){65536u, 256u}, fixture->lexer_frames,
      TEST_LEX_FRAMES, fixture->tokens, TEST_TOKENS, fixture->nodes,
      TEST_NODES, fixture->parse_frames, TEST_PARSE_FRAMES, fixture->issues,
      TEST_ISSUES, &fixture->parser, &lex_error));
  CHECK(w_seed_parser_parse(&fixture->parser, &fixture->parse));
  CHECK(fixture->parse.status == W_SEED_PARSE_COMPLETE &&
        fixture->parse.issue_count == 0u);
  fixture->document.logical_source_id = source_id;
  fixture->document.module_id = module_id;
  fixture->document.local_module_name = local_name;
  fixture->document.source = &fixture->source;
  fixture->document.nodes = fixture->nodes;
  fixture->document.node_count = fixture->parse.node_count;
  fixture->document.parse = fixture->parse;
  return true;
}

static void source_digest(const w_seed_source *source, uint8_t digest[32]) {
  static const uint8_t tag[] = "w-module-source-v1\0";
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, tag, sizeof(tag) - 1u);
  w_seed_sha256_update(&state, source->bytes.data, source->bytes.length);
  w_seed_sha256_final(&state, digest);
}

static void set_facts(w_seed_ephemeral_graph_provider_facts *facts,
                      const w_seed_source *source, const char *canonical) {
  *facts = (w_seed_ephemeral_graph_provider_facts){
      {(const char *)"provider", 8u},
      {(const char *)"root", 4u},
      {(const char *)"owner", 5u},
      {canonical, strlen(canonical)},
      true,
      true,
      W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE,
      source->bytes.length,
      {0u},
      source->bytes.length,
      {0u},
  };
  source_digest(source, facts->snapshot_before_digest);
  (void)memcpy(facts->snapshot_after_digest, facts->snapshot_before_digest,
               sizeof(facts->snapshot_after_digest));
}

static bool make_positive(document_fixture fixtures[3],
                          w_seed_ephemeral_graph_input *input,
                          w_seed_ephemeral_graph_provider_facts facts[3],
                          test_graph_scratch *storage) {
  CHECK(parse_document(&fixtures[0],
                       "module app\nimport command;\nimport platform.native;\n",
                       (w_seed_frontend_text){"root.w", 6u},
                       (w_seed_frontend_text){"app", 3u},
                       (w_seed_frontend_text){"app", 3u}));
  CHECK(parse_document(&fixtures[1], "import platform.native;\n",
                       (w_seed_frontend_text){"command.w", 9u},
                       (w_seed_frontend_text){"command", 7u},
                       (w_seed_frontend_text){"command", 7u}));
  CHECK(parse_document(&fixtures[2], "\n",
                       (w_seed_frontend_text){"platform/native.w", 17u},
                       (w_seed_frontend_text){"platform.native", 15u},
                       (w_seed_frontend_text){"native", 6u}));
  set_facts(&facts[0], &fixtures[0].source, "canonical-root");
  set_facts(&facts[1], &fixtures[1].source, "canonical-command");
  set_facts(&facts[2], &fixtures[2].source, "canonical-native");
  *input = (w_seed_ephemeral_graph_input){
      NULL, facts, 3u, 0u, &storage->scratch, 64u, 4096u, 64u,
      16u * 1024u * 1024u};
  return true;
}

static bool test_transitive_and_frontend(void) {
  document_fixture fixtures[3];
  w_seed_frontend_document documents[3];
  w_seed_ephemeral_graph_provider_facts facts[3];
  test_graph_scratch storage;
  w_seed_ephemeral_graph_input input;
  init_graph_scratch(&storage, 3u, 3u, 2u);
  CHECK(make_positive(fixtures, &input, facts, &storage));
  for (size_t index = 0u; index < 3u; index += 1u)
    documents[index] = fixtures[index].document;
  input.documents = documents;
  /* The documents are contiguous in candidate order for this first run. */
  w_seed_ephemeral_graph_inventory_item inventory[3];
  w_seed_ephemeral_graph_edge edges[3];
  uint32_t document_order[3];
  w_seed_frontend_resolved_import resolved[3];
  w_seed_ephemeral_graph_output output = {
      inventory, 3u, edges, 3u, document_order, 3u, resolved, 3u};
  w_seed_ephemeral_graph_result result;
  CHECK(w_seed_ephemeral_graph_write(&input, &output, &result) ==
        W_SEED_EPHEMERAL_GRAPH_OK);
  CHECK(result.required.sources == 3u && result.required.edges == 3u);
  CHECK(inventory[0].candidate_index == 0u &&
        inventory[0].source_id.length == 6u);
  CHECK(inventory[1].source_id.length == 9u &&
        memcmp(inventory[1].source_id.data, "command.w", 9u) == 0);
  CHECK(inventory[2].source_id.length == 17u &&
        memcmp(inventory[2].source_id.data, "platform/native.w", 17u) == 0);
  CHECK(edges[0].source_ordinal == 0u && edges[0].target_ordinal == 1u &&
        edges[0].direct_import_ordinal == 0u);
  CHECK(edges[1].source_ordinal == 0u && edges[1].target_ordinal == 2u &&
        edges[1].direct_import_ordinal == 1u);
  CHECK(edges[2].source_ordinal == 1u && edges[2].target_ordinal == 2u &&
        edges[2].direct_import_ordinal == 0u);
  CHECK(document_order[0] == 0u && document_order[1] == 1u &&
        document_order[2] == 2u);
  w_seed_frontend_document ordered_documents[3];
  for (size_t index = 0u; index < 3u; index += 1u)
    ordered_documents[index] = fixtures[document_order[index]].document;
  w_seed_frontend_input frontend_input = {
      ordered_documents, 3u, NULL, 0u, true, resolved, 3u};
  w_seed_frontend_counts counts;
  w_seed_frontend_result frontend_result;
  CHECK(w_seed_frontend_measure(&frontend_input, &counts, &frontend_result) ==
        W_SEED_FRONTEND_OK);
  return true;
}

static bool test_unreachable_and_capacity(void) {
  document_fixture fixtures[4];
  w_seed_frontend_document documents[4];
  w_seed_ephemeral_graph_provider_facts facts[4];
  test_graph_scratch storage;
  w_seed_ephemeral_graph_input input;
  init_graph_scratch(&storage, 3u, 3u, 2u);
  CHECK(make_positive(fixtures, &input, facts, &storage));
  CHECK(parse_document(&fixtures[3], "import missing;\n",
                       (w_seed_frontend_text){NULL, 0u},
                       (w_seed_frontend_text){NULL, 0u},
                       (w_seed_frontend_text){NULL, 0u}));
  facts[3] = (w_seed_ephemeral_graph_provider_facts){0};
  for (size_t index = 0u; index < 4u; index += 1u)
    documents[index] = fixtures[index].document;
  input.documents = documents;
  input.candidate_count = 4u;
  w_seed_ephemeral_graph_counts counts;
  w_seed_ephemeral_graph_result result;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_OK);
  CHECK(counts.sources == 3u && counts.edges == 3u);
  input.max_sources = 2u;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_CAPACITY);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT);
  return true;
}

static bool single_graph(document_fixture *fixture,
                         w_seed_frontend_document *document,
                         w_seed_ephemeral_graph_provider_facts *facts,
                         w_seed_ephemeral_graph_input *input,
                         test_graph_scratch *storage,
                         const char *source_text,
                         w_seed_frontend_text source_id,
                         w_seed_frontend_text module_id,
                         w_seed_frontend_text local_name) {
  CHECK(parse_document(fixture, source_text, source_id, module_id, local_name));
  *document = fixture->document;
  set_facts(facts, &fixture->source, "single-canonical");
  *input = (w_seed_ephemeral_graph_input){
      document, facts, 1u, 0u, &storage->scratch, 64u, 4096u, 64u,
      16u * 1024u * 1024u};
  return true;
}

static bool test_digest_and_adversarial_barriers(void) {
  document_fixture fixture;
  w_seed_frontend_document document;
  w_seed_ephemeral_graph_provider_facts facts;
  w_seed_ephemeral_graph_input input;
  w_seed_ephemeral_graph_counts counts;
  w_seed_ephemeral_graph_result result;
  test_graph_scratch storage;
  init_graph_scratch(&storage, 1u, 1u, 1u);
  CHECK(single_graph(&fixture, &document, &facts, &input, &storage, "",
                     (w_seed_frontend_text){"empty.w", 7u},
                     (w_seed_frontend_text){"empty", 5u},
                     (w_seed_frontend_text){"empty", 5u}));
  w_seed_ephemeral_graph_inventory_item inventory[1];
  uint32_t document_order[1];
  w_seed_ephemeral_graph_output output = {inventory, 1u, NULL, 0u, document_order, 1u,
                                          NULL, 0u};
  CHECK(w_seed_ephemeral_graph_write(&input, &output, &result) ==
        W_SEED_EPHEMERAL_GRAPH_OK);
  static const uint8_t expected_digest[32] = {
      0xbcu, 0x76u, 0x15u, 0x60u, 0x67u, 0x88u, 0xefu, 0x87u,
      0x80u, 0x3cu, 0x33u, 0x4au, 0xa1u, 0x05u, 0xb2u, 0xb7u,
      0x88u, 0xc4u, 0xcbu, 0x25u, 0xdau, 0x5du, 0x81u, 0xb0u,
      0xfeu, 0x3fu, 0x27u, 0xe8u, 0x84u, 0x37u, 0x07u, 0x76u};
  CHECK(result.required.sources == 1u && result.required.edges == 0u);
  CHECK(memcmp(inventory[0].digest, expected_digest,
               sizeof(expected_digest)) == 0);
  (void)printf("RESULT source-digest=");
  for (size_t index = 0u; index < sizeof(inventory[0].digest); index += 1u)
    (void)printf("%02x", (unsigned int)inventory[0].digest[index]);
  (void)printf("\n");

  facts.opened = false;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_PROVIDER);
  facts.opened = true;
  facts.containment_inside = false;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_PROVIDER);
  facts.containment_inside = true;
  facts.symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_ESCAPE;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_PROVIDER);
  facts.symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  facts.snapshot_after_byte_count += 1u;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_SNAPSHOT);
  facts.snapshot_after_byte_count -= 1u;
  facts.snapshot_before_digest[0] ^= 1u;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_SNAPSHOT);
  facts.snapshot_before_digest[0] ^= 1u;

  static const char non_ascii_name[] = "r\xc3\xb6ot.w";
  document.logical_source_id = (w_seed_frontend_text){non_ascii_name, 7u};
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_UNSUPPORTED_NFC);
  document.logical_source_id = (w_seed_frontend_text){"empty.w", 7u};
  static const char malformed_name[] = "../empty.w";
  document.logical_source_id =
      (w_seed_frontend_text){malformed_name, sizeof(malformed_name) - 1u};
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_PATH);

  char mutable_source[] = "\n";
  CHECK(single_graph(&fixture, &document, &facts, &input, &storage,
                     mutable_source,
                     (w_seed_frontend_text){"empty.w", 7u},
                     (w_seed_frontend_text){"empty", 5u},
                     (w_seed_frontend_text){"empty", 5u}));
  mutable_source[0] = ' ';
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_SNAPSHOT);
  return true;
}

static bool test_std_missing_cycle_and_all_or_nothing(void) {
  document_fixture fixture;
  w_seed_frontend_document document;
  w_seed_ephemeral_graph_provider_facts facts;
  w_seed_ephemeral_graph_input input;
  w_seed_ephemeral_graph_counts counts;
  w_seed_ephemeral_graph_result result;
  test_graph_scratch storage;
  init_graph_scratch(&storage, 2u, 1u, 1u);
  CHECK(single_graph(&fixture, &document, &facts, &input, &storage,
                     "module app\nimport std.io;\n",
                     (w_seed_frontend_text){"app.w", 5u},
                     (w_seed_frontend_text){"app", 3u},
                     (w_seed_frontend_text){"app", 3u}));
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_STD_PROVIDER);
  CHECK(single_graph(&fixture, &document, &facts, &input, &storage,
                     "module app\nimport missing;\n",
                     (w_seed_frontend_text){"app.w", 5u},
                     (w_seed_frontend_text){"app", 3u},
                     (w_seed_frontend_text){"app", 3u}));
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_MISSING_LOCAL);
  CHECK(single_graph(&fixture, &document, &facts, &input, &storage,
                     "module app\nimport app;\n",
                     (w_seed_frontend_text){"app.w", 5u},
                     (w_seed_frontend_text){"app", 3u},
                     (w_seed_frontend_text){"app", 3u}));
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_CYCLE);

  w_seed_ephemeral_graph_inventory_item inventory[1];
  uint8_t sentinel_inventory[sizeof(inventory)];
  (void)memset(inventory, 0xa5, sizeof(inventory));
  (void)memcpy(sentinel_inventory, inventory, sizeof(inventory));
  CHECK(single_graph(&fixture, &document, &facts, &input, &storage,
                     "module app\n",
                     (w_seed_frontend_text){"app.w", 5u},
                     (w_seed_frontend_text){"app", 3u},
                     (w_seed_frontend_text){"app", 3u}));
  w_seed_ephemeral_graph_output short_output = {inventory, 0u, NULL, 0u,
                                                NULL, 0u, NULL, 0u};
  CHECK(w_seed_ephemeral_graph_write(&input, &short_output, &result) ==
        W_SEED_EPHEMERAL_GRAPH_CAPACITY);
  CHECK(result.required.sources == 1u &&
        memcmp(inventory, sentinel_inventory, sizeof(inventory)) == 0);

  document_fixture child_fixture;
  w_seed_frontend_document child_document;
  w_seed_ephemeral_graph_provider_facts child_facts;
  CHECK(parse_document(&child_fixture, "\n",
                       (w_seed_frontend_text){"child.w", 7u},
                       (w_seed_frontend_text){"child", 5u},
                       (w_seed_frontend_text){"child", 5u}));
  child_document = child_fixture.document;
  set_facts(&child_facts, &child_fixture.source, "child-canonical");
  w_seed_frontend_document two_documents[2] = {document, child_document};
  w_seed_ephemeral_graph_provider_facts two_facts[2] = {facts, child_facts};
  /* Replace the root CST with one direct child import for limit checks. */
  CHECK(parse_document(&fixture, "module app\nimport child;\n",
                       (w_seed_frontend_text){"app.w", 5u},
                       (w_seed_frontend_text){"app", 3u},
                       (w_seed_frontend_text){"app", 3u}));
  two_documents[0] = fixture.document;
  set_facts(&two_facts[0], &fixture.source, "root-canonical");
  input.documents = two_documents;
  input.provider_facts = two_facts;
  input.candidate_count = 2u;
  input.max_sources = 1u;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_CAPACITY);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT);
  input.max_sources = 64u;
  input.max_edges = 0u;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_CAPACITY);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT);
  input.max_edges = 4096u;
  input.max_depth = 0u;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_CAPACITY);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT);
  input.max_depth = 64u;
  input.max_total_source_bytes = 0u;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_CAPACITY);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT);

  input.root_candidate_index = 2u;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_INDEX);
  return true;
}

static bool test_scratch_capacity_and_zero_depth(void) {
  document_fixture root_fixture;
  document_fixture child_fixture;
  w_seed_frontend_document documents[2];
  w_seed_ephemeral_graph_provider_facts facts[2];
  CHECK(parse_document(&root_fixture, "module app\nimport child;\n",
                       (w_seed_frontend_text){"app.w", 5u},
                       (w_seed_frontend_text){"app", 3u},
                       (w_seed_frontend_text){"app", 3u}));
  CHECK(parse_document(&child_fixture, "\n",
                       (w_seed_frontend_text){"child.w", 7u},
                       (w_seed_frontend_text){"child", 5u},
                       (w_seed_frontend_text){"child", 5u}));
  documents[0] = root_fixture.document;
  documents[1] = child_fixture.document;
  set_facts(&facts[0], &root_fixture.source, "scratch-root");
  set_facts(&facts[1], &child_fixture.source, "scratch-child");
  test_graph_scratch full_storage;
  test_graph_scratch short_storage;
  test_graph_scratch zero_storage;
  init_graph_scratch(&full_storage, 2u, 1u, 1u);
  init_graph_scratch(&short_storage, 1u, 1u, 1u);
  init_graph_scratch(&zero_storage, 0u, 0u, 0u);
  w_seed_ephemeral_graph_input input = {
      documents, facts, 2u, 0u, &full_storage.scratch, 64u, 4096u, 64u,
      16u * 1024u * 1024u};
  w_seed_ephemeral_graph_inventory_item inventory[2];
  w_seed_ephemeral_graph_edge edges[1];
  uint32_t document_order[2];
  w_seed_frontend_resolved_import resolved[1];
  w_seed_ephemeral_graph_output output = {
      inventory, 2u, edges, 1u, document_order, 2u, resolved, 1u};
  uint8_t before_inventory[sizeof(inventory)];
  uint8_t before_edges[sizeof(edges)];
  uint8_t before_order[sizeof(document_order)];
  uint8_t before_resolved[sizeof(resolved)];
  w_seed_ephemeral_graph_counts counts;
  w_seed_ephemeral_graph_result result;

  input.scratch = &short_storage.scratch;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_CAPACITY);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT);
  (void)memset(inventory, 0xa1, sizeof(inventory));
  (void)memset(edges, 0xa2, sizeof(edges));
  (void)memset(document_order, 0xa3, sizeof(document_order));
  (void)memset(resolved, 0xa4, sizeof(resolved));
  (void)memcpy(before_inventory, inventory, sizeof(inventory));
  (void)memcpy(before_edges, edges, sizeof(edges));
  (void)memcpy(before_order, document_order, sizeof(document_order));
  (void)memcpy(before_resolved, resolved, sizeof(resolved));
  CHECK(w_seed_ephemeral_graph_write(&input, &output, &result) ==
        W_SEED_EPHEMERAL_GRAPH_CAPACITY);
  CHECK(memcmp(inventory, before_inventory, sizeof(inventory)) == 0 &&
        memcmp(edges, before_edges, sizeof(edges)) == 0 &&
        memcmp(document_order, before_order, sizeof(document_order)) == 0 &&
        memcmp(resolved, before_resolved, sizeof(resolved)) == 0);

  input.scratch = &zero_storage.scratch;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_CAPACITY);
  (void)memset(inventory, 0xb1, sizeof(inventory));
  (void)memset(edges, 0xb2, sizeof(edges));
  (void)memset(document_order, 0xb3, sizeof(document_order));
  (void)memset(resolved, 0xb4, sizeof(resolved));
  (void)memcpy(before_inventory, inventory, sizeof(inventory));
  (void)memcpy(before_edges, edges, sizeof(edges));
  (void)memcpy(before_order, document_order, sizeof(document_order));
  (void)memcpy(before_resolved, resolved, sizeof(resolved));
  CHECK(w_seed_ephemeral_graph_write(&input, &output, &result) ==
        W_SEED_EPHEMERAL_GRAPH_CAPACITY);
  CHECK(memcmp(inventory, before_inventory, sizeof(inventory)) == 0 &&
        memcmp(edges, before_edges, sizeof(edges)) == 0 &&
        memcmp(document_order, before_order, sizeof(document_order)) == 0 &&
        memcmp(resolved, before_resolved, sizeof(resolved)) == 0);

  test_graph_scratch overlap_storage;
  init_graph_scratch(&overlap_storage, 2u, 1u, 1u);
  overlap_storage.scratch.edges =
      (w_seed_ephemeral_graph_scratch_edge *)(void *)overlap_storage.nodes;
  input.scratch = &overlap_storage.scratch;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_GRAPH_FAILURE_SCRATCH);

  CHECK(parse_document(&root_fixture, "module app\n",
                       (w_seed_frontend_text){"app.w", 5u},
                       (w_seed_frontend_text){"app", 3u},
                       (w_seed_frontend_text){"app", 3u}));
  documents[0] = root_fixture.document;
  set_facts(&facts[0], &root_fixture.source, "scratch-root-only");
  input.documents = documents;
  input.provider_facts = facts;
  input.candidate_count = 1u;
  input.scratch = &full_storage.scratch;
  input.max_depth = 0u;
  input.max_edges = 0u;
  input.max_total_source_bytes = 16u * 1024u * 1024u;
  CHECK(w_seed_ephemeral_graph_measure(&input, &counts, &result) ==
        W_SEED_EPHEMERAL_GRAPH_OK);
  CHECK(counts.sources == 1u && counts.edges == 0u);
  return true;
}

static bool test_candidate_order_determinism(void) {
  document_fixture first_fixtures[3];
  document_fixture second_fixtures[3];
  w_seed_frontend_document first_documents[3];
  w_seed_frontend_document second_documents[3];
  w_seed_ephemeral_graph_provider_facts first_facts[3];
  w_seed_ephemeral_graph_provider_facts second_facts[3];
  w_seed_ephemeral_graph_input first_input;
  w_seed_ephemeral_graph_input second_input;
  test_graph_scratch first_storage;
  test_graph_scratch second_storage;
  init_graph_scratch(&first_storage, 3u, 3u, 2u);
  init_graph_scratch(&second_storage, 3u, 3u, 2u);
  CHECK(make_positive(first_fixtures, &first_input, first_facts,
                      &first_storage));
  for (size_t index = 0u; index < 3u; index += 1u)
    first_documents[index] = first_fixtures[index].document;
  first_input.documents = first_documents;
  CHECK(parse_document(&second_fixtures[0],
                       "module app\nimport command;\nimport platform.native;\n",
                       (w_seed_frontend_text){"root.w", 6u},
                       (w_seed_frontend_text){"app", 3u},
                       (w_seed_frontend_text){"app", 3u}));
  CHECK(parse_document(&second_fixtures[1], "\n",
                       (w_seed_frontend_text){"platform/native.w", 17u},
                       (w_seed_frontend_text){"platform.native", 15u},
                       (w_seed_frontend_text){"native", 6u}));
  CHECK(parse_document(&second_fixtures[2], "import platform.native;\n",
                       (w_seed_frontend_text){"command.w", 9u},
                       (w_seed_frontend_text){"command", 7u},
                       (w_seed_frontend_text){"command", 7u}));
  set_facts(&second_facts[0], &second_fixtures[0].source, "other-root");
  set_facts(&second_facts[1], &second_fixtures[1].source, "other-native");
  set_facts(&second_facts[2], &second_fixtures[2].source, "other-command");
  for (size_t index = 0u; index < 3u; index += 1u)
    second_documents[index] = second_fixtures[index].document;
  second_input = (w_seed_ephemeral_graph_input){
      second_documents, second_facts, 3u, 0u, &second_storage.scratch, 64u,
      4096u, 64u,
      16u * 1024u * 1024u};
  w_seed_ephemeral_graph_inventory_item first_inventory[3];
  w_seed_ephemeral_graph_inventory_item second_inventory[3];
  w_seed_ephemeral_graph_edge first_edges[3];
  w_seed_ephemeral_graph_edge second_edges[3];
  uint32_t first_order[3];
  uint32_t second_order[3];
  w_seed_frontend_resolved_import first_resolved[3];
  w_seed_frontend_resolved_import second_resolved[3];
  w_seed_ephemeral_graph_output first_output = {
      first_inventory, 3u, first_edges, 3u, first_order, 3u, first_resolved, 3u};
  w_seed_ephemeral_graph_output second_output = {
      second_inventory, 3u, second_edges, 3u, second_order, 3u,
      second_resolved, 3u};
  w_seed_ephemeral_graph_result first_result;
  w_seed_ephemeral_graph_result second_result;
  CHECK(w_seed_ephemeral_graph_write(&first_input, &first_output, &first_result) ==
        W_SEED_EPHEMERAL_GRAPH_OK);
  CHECK(w_seed_ephemeral_graph_write(&second_input, &second_output,
                                     &second_result) ==
        W_SEED_EPHEMERAL_GRAPH_OK);
  for (size_t index = 0u; index < 3u; index += 1u) {
    CHECK(first_inventory[index].source_id.length ==
          second_inventory[index].source_id.length);
    CHECK(memcmp(first_inventory[index].source_id.data,
                 second_inventory[index].source_id.data,
                 first_inventory[index].source_id.length) == 0);
    CHECK(memcmp(first_inventory[index].digest, second_inventory[index].digest,
                 sizeof(first_inventory[index].digest)) == 0);
    CHECK(first_edges[index].source_ordinal == second_edges[index].source_ordinal &&
          first_edges[index].target_ordinal == second_edges[index].target_ordinal &&
          first_edges[index].direct_import_ordinal ==
              second_edges[index].direct_import_ordinal);
  }
  CHECK(first_inventory[0].candidate_index == 0u &&
        second_inventory[0].candidate_index == 0u &&
        first_inventory[1].candidate_index != second_inventory[1].candidate_index);
  return true;
}

static bool test_import_order_projections(void) {
  document_fixture first_fixtures[3];
  document_fixture second_fixtures[3];
  w_seed_frontend_document first_documents[3];
  w_seed_frontend_document second_documents[3];
  w_seed_ephemeral_graph_provider_facts first_facts[3];
  w_seed_ephemeral_graph_provider_facts second_facts[3];
  CHECK(parse_document(&first_fixtures[0],
                       "module app\nimport zeta;\nimport alpha;\n",
                       (w_seed_frontend_text){"app.w", 5u},
                       (w_seed_frontend_text){"app", 3u},
                       (w_seed_frontend_text){"app", 3u}));
  CHECK(parse_document(&first_fixtures[1], "\n",
                       (w_seed_frontend_text){"zeta.w", 6u},
                       (w_seed_frontend_text){"zeta", 4u},
                       (w_seed_frontend_text){"zeta", 4u}));
  CHECK(parse_document(&first_fixtures[2], "\n",
                       (w_seed_frontend_text){"alpha.w", 7u},
                       (w_seed_frontend_text){"alpha", 5u},
                       (w_seed_frontend_text){"alpha", 5u}));
  CHECK(parse_document(&second_fixtures[0],
                       "module app\nimport alpha;\nimport zeta;\n",
                       (w_seed_frontend_text){"app.w", 5u},
                       (w_seed_frontend_text){"app", 3u},
                       (w_seed_frontend_text){"app", 3u}));
  CHECK(parse_document(&second_fixtures[1], "\n",
                       (w_seed_frontend_text){"alpha.w", 7u},
                       (w_seed_frontend_text){"alpha", 5u},
                       (w_seed_frontend_text){"alpha", 5u}));
  CHECK(parse_document(&second_fixtures[2], "\n",
                       (w_seed_frontend_text){"zeta.w", 6u},
                       (w_seed_frontend_text){"zeta", 4u},
                       (w_seed_frontend_text){"zeta", 4u}));
  for (size_t index = 0u; index < 3u; index += 1u) {
    first_documents[index] = first_fixtures[index].document;
    second_documents[index] = second_fixtures[index].document;
  }
  set_facts(&first_facts[0], &first_fixtures[0].source, "first-root");
  set_facts(&first_facts[1], &first_fixtures[1].source, "first-zeta");
  set_facts(&first_facts[2], &first_fixtures[2].source, "first-alpha");
  set_facts(&second_facts[0], &second_fixtures[0].source, "second-root");
  set_facts(&second_facts[1], &second_fixtures[1].source, "second-alpha");
  set_facts(&second_facts[2], &second_fixtures[2].source, "second-zeta");
  test_graph_scratch first_storage;
  test_graph_scratch second_storage;
  init_graph_scratch(&first_storage, 3u, 2u, 2u);
  init_graph_scratch(&second_storage, 3u, 2u, 2u);
  const w_seed_ephemeral_graph_input first_input = {
      first_documents, first_facts, 3u, 0u, &first_storage.scratch, 64u, 4096u,
      64u,
      16u * 1024u * 1024u};
  const w_seed_ephemeral_graph_input second_input = {
      second_documents, second_facts, 3u, 0u, &second_storage.scratch, 64u,
      4096u, 64u,
      16u * 1024u * 1024u};
  w_seed_ephemeral_graph_inventory_item first_inventory[3];
  w_seed_ephemeral_graph_inventory_item second_inventory[3];
  w_seed_ephemeral_graph_edge first_edges[2];
  w_seed_ephemeral_graph_edge second_edges[2];
  uint32_t first_order[3];
  uint32_t second_order[3];
  w_seed_frontend_resolved_import first_resolved[2];
  w_seed_frontend_resolved_import second_resolved[2];
  w_seed_ephemeral_graph_output first_output = {
      first_inventory, 3u, first_edges, 2u, first_order, 3u, first_resolved, 2u};
  w_seed_ephemeral_graph_output second_output = {
      second_inventory, 3u, second_edges, 2u, second_order, 3u,
      second_resolved, 2u};
  w_seed_ephemeral_graph_result first_result;
  w_seed_ephemeral_graph_result second_result;
  CHECK(w_seed_ephemeral_graph_write(&first_input, &first_output, &first_result) ==
        W_SEED_EPHEMERAL_GRAPH_OK);
  CHECK(w_seed_ephemeral_graph_write(&second_input, &second_output,
                                     &second_result) ==
        W_SEED_EPHEMERAL_GRAPH_OK);
  CHECK(first_edges[0].target_ordinal == second_edges[0].target_ordinal &&
        first_edges[1].target_ordinal == second_edges[1].target_ordinal);
  const w_seed_frontend_text first_edge_zero_path = {
      (const char *)(first_fixtures[0].source.bytes.data +
                     first_edges[0].path_span.start_byte),
      first_edges[0].path_span.end_byte - first_edges[0].path_span.start_byte};
  const w_seed_frontend_text second_edge_zero_path = {
      (const char *)(second_fixtures[0].source.bytes.data +
                     second_edges[0].path_span.start_byte),
      second_edges[0].path_span.end_byte - second_edges[0].path_span.start_byte};
  CHECK(first_edge_zero_path.length == 5u &&
        second_edge_zero_path.length == 5u &&
        memcmp(first_edge_zero_path.data, "alpha", 5u) == 0 &&
        memcmp(second_edge_zero_path.data, "alpha", 5u) == 0);
  CHECK(first_edges[0].direct_import_ordinal == 1u &&
        second_edges[0].direct_import_ordinal == 0u);
  CHECK(first_resolved[0].direct_import_ordinal == 0u &&
        first_resolved[0].target_index == 2u &&
        first_resolved[1].direct_import_ordinal == 1u &&
        first_resolved[1].target_index == 1u);
  CHECK(second_resolved[0].direct_import_ordinal == 0u &&
        second_resolved[0].target_index == 1u &&
        second_resolved[1].direct_import_ordinal == 1u &&
        second_resolved[1].target_index == 2u);
  return true;
}

static bool test_source_id_byte_order(void) {
  document_fixture fixtures[3];
  w_seed_frontend_document documents[3];
  w_seed_ephemeral_graph_provider_facts facts[3];
  test_graph_scratch storage;
  init_graph_scratch(&storage, 3u, 2u, 2u);
  CHECK(parse_document(&fixtures[0],
                       "module root\nimport b;\nimport aa;\n",
                       (w_seed_frontend_text){"root.w", 6u},
                       (w_seed_frontend_text){"root", 4u},
                       (w_seed_frontend_text){"root", 4u}));
  CHECK(parse_document(&fixtures[1], "\n",
                       (w_seed_frontend_text){"b.w", 3u},
                       (w_seed_frontend_text){"b", 1u},
                       (w_seed_frontend_text){"b", 1u}));
  CHECK(parse_document(&fixtures[2], "\n",
                       (w_seed_frontend_text){"aa.w", 4u},
                       (w_seed_frontend_text){"aa", 2u},
                       (w_seed_frontend_text){"aa", 2u}));
  for (size_t index = 0u; index < 3u; index += 1u) {
    documents[index] = fixtures[index].document;
    set_facts(&facts[index], &fixtures[index].source, index == 0u
                                                    ? "byte-root"
                                                    : (index == 1u ? "byte-b"
                                                                   : "byte-aa"));
  }
  const w_seed_ephemeral_graph_input input = {
      documents, facts, 3u, 0u, &storage.scratch, 64u, 4096u, 64u,
      16u * 1024u * 1024u};
  w_seed_ephemeral_graph_inventory_item inventory[3];
  w_seed_ephemeral_graph_edge edges[2];
  uint32_t document_order[3];
  w_seed_frontend_resolved_import resolved[2];
  w_seed_ephemeral_graph_output output = {
      inventory, 3u, edges, 2u, document_order, 3u, resolved, 2u};
  w_seed_ephemeral_graph_result result;
  CHECK(w_seed_ephemeral_graph_write(&input, &output, &result) ==
        W_SEED_EPHEMERAL_GRAPH_OK);
  CHECK(inventory[1].source_id.length == 4u &&
        memcmp(inventory[1].source_id.data, "aa.w", 4u) == 0 &&
        inventory[2].source_id.length == 3u &&
        memcmp(inventory[2].source_id.data, "b.w", 3u) == 0);
  CHECK(edges[0].target_ordinal == 1u &&
        edges[0].direct_import_ordinal == 1u &&
        edges[1].target_ordinal == 2u &&
        edges[1].direct_import_ordinal == 0u);
  CHECK(resolved[0].direct_import_ordinal == 0u &&
        resolved[0].target_index == 2u &&
        resolved[1].direct_import_ordinal == 1u &&
        resolved[1].target_index == 1u);
  return true;
}

static bool test_shuffled_root_candidate_and_frontend(void) {
  document_fixture root_fixture;
  document_fixture command_fixture;
  document_fixture native_fixture;
  w_seed_frontend_document documents[3];
  w_seed_ephemeral_graph_provider_facts facts[3];
  test_graph_scratch storage;
  init_graph_scratch(&storage, 3u, 3u, 2u);
  CHECK(parse_document(&root_fixture,
                       "module app\nimport command;\nimport platform.native;\n",
                       (w_seed_frontend_text){"root.w", 6u},
                       (w_seed_frontend_text){"app", 3u},
                       (w_seed_frontend_text){"app", 3u}));
  CHECK(parse_document(&command_fixture, "import platform.native;\n",
                       (w_seed_frontend_text){"command.w", 9u},
                       (w_seed_frontend_text){"command", 7u},
                       (w_seed_frontend_text){"command", 7u}));
  CHECK(parse_document(&native_fixture, "\n",
                       (w_seed_frontend_text){"platform/native.w", 17u},
                       (w_seed_frontend_text){"platform.native", 15u},
                       (w_seed_frontend_text){"native", 6u}));

  /* Candidate order is platform, command, root; root_candidate_index is 2. */
  documents[0] = native_fixture.document;
  documents[1] = command_fixture.document;
  documents[2] = root_fixture.document;
  set_facts(&facts[0], &native_fixture.source, "shuffled-native");
  set_facts(&facts[1], &command_fixture.source, "shuffled-command");
  set_facts(&facts[2], &root_fixture.source, "shuffled-root");
  const w_seed_ephemeral_graph_input input = {
      documents, facts, 3u, 2u, &storage.scratch, 64u, 4096u, 64u,
      16u * 1024u * 1024u};
  w_seed_ephemeral_graph_inventory_item inventory[3];
  w_seed_ephemeral_graph_edge edges[3];
  uint32_t document_order[3];
  w_seed_frontend_resolved_import resolved[3];
  w_seed_ephemeral_graph_output output = {
      inventory, 3u, edges, 3u, document_order, 3u, resolved, 3u};
  w_seed_ephemeral_graph_result result;
  CHECK(w_seed_ephemeral_graph_write(&input, &output, &result) ==
        W_SEED_EPHEMERAL_GRAPH_OK);
  CHECK(inventory[0].candidate_index == 2u &&
        memcmp(inventory[0].source_id.data, "root.w", 6u) == 0);
  CHECK(inventory[1].candidate_index == 1u &&
        memcmp(inventory[1].source_id.data, "command.w", 9u) == 0);
  CHECK(inventory[2].candidate_index == 0u &&
        memcmp(inventory[2].source_id.data, "platform/native.w", 17u) == 0);
  CHECK(document_order[0] == 2u && document_order[1] == 1u &&
        document_order[2] == 0u);
  CHECK(edges[0].source_ordinal == 0u && edges[0].target_ordinal == 1u &&
        edges[1].source_ordinal == 0u && edges[1].target_ordinal == 2u &&
        edges[2].source_ordinal == 1u && edges[2].target_ordinal == 2u);
  CHECK(resolved[0].source_document_index == 0u &&
        resolved[0].direct_import_ordinal == 0u &&
        resolved[0].target_index == 1u);
  CHECK(resolved[1].source_document_index == 0u &&
        resolved[1].direct_import_ordinal == 1u &&
        resolved[1].target_index == 2u);
  CHECK(resolved[2].source_document_index == 1u &&
        resolved[2].direct_import_ordinal == 0u &&
        resolved[2].target_index == 2u);

  w_seed_frontend_document ordered_documents[3];
  for (size_t index = 0u; index < 3u; index += 1u)
    ordered_documents[index] = documents[document_order[index]];
  const w_seed_frontend_input frontend_input = {
      ordered_documents, 3u, NULL, 0u, true, resolved, 3u};
  w_seed_frontend_counts frontend_counts;
  w_seed_frontend_result frontend_result;
  CHECK(w_seed_frontend_measure(&frontend_input, &frontend_counts,
                                &frontend_result) == W_SEED_FRONTEND_OK);
  return true;
}

static bool test_duplicate_edge_total_order(void) {
  document_fixture fixtures[3];
  w_seed_frontend_document documents[3];
  w_seed_ephemeral_graph_provider_facts facts[3];
  test_graph_scratch storage;
  init_graph_scratch(&storage, 3u, 3u, 3u);
  CHECK(parse_document(&fixtures[0],
                       "module app\nimport alpha;\nimport alpha;\nimport zeta;\n",
                       (w_seed_frontend_text){"app.w", 5u},
                       (w_seed_frontend_text){"app", 3u},
                       (w_seed_frontend_text){"app", 3u}));
  CHECK(parse_document(&fixtures[1], "\n",
                       (w_seed_frontend_text){"alpha.w", 7u},
                       (w_seed_frontend_text){"alpha", 5u},
                       (w_seed_frontend_text){"alpha", 5u}));
  CHECK(parse_document(&fixtures[2], "\n",
                       (w_seed_frontend_text){"zeta.w", 6u},
                       (w_seed_frontend_text){"zeta", 4u},
                       (w_seed_frontend_text){"zeta", 4u}));
  for (size_t index = 0u; index < 3u; index += 1u) {
    documents[index] = fixtures[index].document;
    set_facts(&facts[index], &fixtures[index].source,
              index == 0u ? "duplicate-root"
                          : (index == 1u ? "duplicate-alpha"
                                         : "duplicate-zeta"));
  }
  const w_seed_ephemeral_graph_input input = {
      documents, facts, 3u, 0u, &storage.scratch, 64u, 4096u, 64u,
      16u * 1024u * 1024u};
  w_seed_ephemeral_graph_inventory_item inventory[3];
  w_seed_ephemeral_graph_edge edges[3];
  uint32_t document_order[3];
  w_seed_frontend_resolved_import resolved[3];
  w_seed_ephemeral_graph_output output = {
      inventory, 3u, edges, 3u, document_order, 3u, resolved, 3u};
  w_seed_ephemeral_graph_result result;
  CHECK(w_seed_ephemeral_graph_write(&input, &output, &result) ==
        W_SEED_EPHEMERAL_GRAPH_OK);
  CHECK(edges[0].target_ordinal == 1u &&
        edges[0].direct_import_ordinal == 0u &&
        edges[1].target_ordinal == 1u &&
        edges[1].direct_import_ordinal == 1u &&
        edges[2].target_ordinal == 2u &&
        edges[2].direct_import_ordinal == 2u);
  CHECK(resolved[0].target_index == 1u &&
        resolved[0].direct_import_ordinal == 0u &&
        resolved[1].target_index == 1u &&
        resolved[1].direct_import_ordinal == 1u &&
        resolved[2].target_index == 2u &&
        resolved[2].direct_import_ordinal == 2u);
  return true;
}

int main(void) {
  CHECK(test_transitive_and_frontend());
  CHECK(test_unreachable_and_capacity());
  CHECK(test_digest_and_adversarial_barriers());
  CHECK(test_std_missing_cycle_and_all_or_nothing());
  CHECK(test_scratch_capacity_and_zero_depth());
  CHECK(test_candidate_order_determinism());
  CHECK(test_import_order_projections());
  CHECK(test_source_id_byte_order());
  CHECK(test_shuffled_root_candidate_and_frontend());
  CHECK(test_duplicate_edge_total_order());
  return 0;
}
