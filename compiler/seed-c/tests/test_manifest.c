#include "w_seed_manifest.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "w_seed_sha256.h"

#define CHECK(value)                                                           \
  do {                                                                         \
    if (!(value)) {                                                            \
      (void)fprintf(stderr, "manifest check failed at line %d\n", __LINE__);   \
      return EXIT_FAILURE;                                                     \
    }                                                                          \
  } while (0)

static bool same_counts(w_seed_manifest_counts left,
                        w_seed_manifest_counts right) {
  return left.documents == right.documents && left.roots == right.roots &&
         left.nodes == right.nodes && left.fields == right.fields &&
         left.edges == right.edges &&
         left.canonical_bytes == right.canonical_bytes &&
         left.structural_nodes == right.structural_nodes;
}

static w_seed_manifest_name_slot
    name_slots[W_SEED_MANIFEST_MAX_STRUCTURAL_NODES];
static uint8_t scalar_bytes[W_SEED_MANIFEST_MAX_DECODED_SCALAR_BYTES +
                            W_SEED_MANIFEST_SCALAR_SCRATCH_OVERHEAD];
static uint8_t source_bytes[W_SEED_MANIFEST_MAX_DOCUMENT_BYTES];
static w_seed_manifest_document output_documents[2];
static w_seed_manifest_root output_roots[4];
static w_seed_manifest_node output_nodes[64];
static w_seed_manifest_field output_fields[64];
static w_seed_manifest_edge output_edges[64];
static uint8_t output_canonical[1024];
static w_seed_manifest_document output_snapshot_documents[2];
static w_seed_manifest_root output_snapshot_roots[4];
static w_seed_manifest_node output_snapshot_nodes[64];
static w_seed_manifest_field output_snapshot_fields[64];
static w_seed_manifest_edge output_snapshot_edges[64];
static uint8_t output_snapshot_canonical[1024];

static uint32_t test_u32_be(uint8_t bytes[4], uint32_t value) {
  bytes[0] = (uint8_t)(value >> 24u);
  bytes[1] = (uint8_t)(value >> 16u);
  bytes[2] = (uint8_t)(value >> 8u);
  bytes[3] = (uint8_t)value;
  return value;
}

static void test_u64_be(uint8_t bytes[8], uint64_t value) {
  for (size_t index = 0u; index < 8u; index += 1u)
    bytes[7u - index] = (uint8_t)(value >> (index * 8u));
}

static void test_source_digest(const uint8_t *bytes, size_t length,
                               uint8_t digest[W_SEED_MANIFEST_DIGEST_BYTES]) {
  static const char tag[] = W_SEED_MANIFEST_DOCUMENT_SOURCE_TAG;
  uint8_t number[8];
  uint8_t tag_length[4];
  test_u32_be(tag_length, (uint32_t)(sizeof(tag) - 1u));
  test_u64_be(number, (uint64_t)length);
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, tag_length, sizeof(tag_length));
  w_seed_sha256_update(&state, (const uint8_t *)tag, sizeof(tag) - 1u);
  w_seed_sha256_update(&state, number, sizeof(number));
  w_seed_sha256_update(&state, bytes, length);
  w_seed_sha256_final(&state, digest);
}

typedef struct {
  uint32_t counts[7];
  const char *source;
  const char *semantic;
  const char *provenance;
  const char *receipt;
} golden_document;

typedef struct {
  const char *name;
  const char *const *sources;
  size_t source_count;
  const golden_document *documents;
  size_t document_count;
  uint32_t counts[7];
  const char *semantic;
  const char *provenance;
  const char *receipt;
  bool check_roots;
  bool check_s5;
} golden_case;

typedef struct {
  w_seed_manifest_result result;
  uint8_t source[2][W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t semantic[2][W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t provenance[2][W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t receipt[2][W_SEED_MANIFEST_DIGEST_BYTES];
} golden_observed;

#define GOLDEN_DOCUMENT(d, r, n, f, e, c, s, source_value, semantic_value,   \
                        provenance_value, receipt_value)                       \
  {{d, r, n, f, e, c, s}, source_value, semantic_value, provenance_value,      \
   receipt_value}

static const char s0_source[] = "package { alpha: 1 beta: \"A\" }\n";
static const char s1_source[] =
    "package {\n  // same semantics\n  beta: \"\\u{41}\",\n  alpha: 1.0e0,\n}\n";
static const char s2_source[] =
    "build { schema: \"w.build/1\" }\npackage {}\n";
static const char s3_source[] = "package { value: [1, 2] }\n";
static const char s4_source[] = "package { value: [2, 1] }\n";
static const char s5_source[] =
    "package { decimal: 1_000.0e+2, text: \"A\\nB\", hex: 0x00_Af }\n";

static const char *const s0_sources[] = {s0_source};
static const char *const s1_sources[] = {s1_source};
static const char *const s2_sources[] = {s2_source};
static const char *const s3_sources[] = {s3_source};
static const char *const s4_sources[] = {s4_source};
static const char *const s5_sources[] = {s5_source};
static const char *const s0_s2_sources[] = {s0_source, s2_source};

static const golden_document s0_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 1u, 3u, 2u, 0u, 83u, 6u,
        "dd8f2091d1d16564ce3d7512bbebbb2bfc8c3a4b5428095f63a14d4a1428b2db",
        "097813a3a02b43766e52ad9a10dbe9644702401161aa11ac2e9ce3838a311c32",
        "238b6943b7658790678abe8c5fd28c56444462c79a3894930bbc6b0276bbd175",
        "1b8cb7d2a1688f4cd63cf8bdcdae1e5f5caef7ac359b691932fa97efa1945b14")};
static const golden_document s1_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 1u, 3u, 2u, 0u, 83u, 6u,
        "87b647cf239df727c3577315dd18571ec0f729398ba9143cb1b88264bf94b85e",
        "097813a3a02b43766e52ad9a10dbe9644702401161aa11ac2e9ce3838a311c32",
        "5a18bbe924d7a43e3b5cfcae338d2da5a9ac7ca099450d911ea360d55f575f43",
        "9eabfd525cef285a482ef6d20d5563e282da33954ef7f99f52bdc70cd3c7d1d0")};
static const golden_document s2_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 2u, 3u, 1u, 0u, 9u, 6u,
        "e30bab1bb290af8c4ff76810787ba585f1e2fb4c8eadb432393f81fe4048ff26",
        "277c96523e1e4d5f8b83d638d05c02d0ce0bb97a0fcdcc76996f8cd9e3b46a91",
        "5198fc25f4ddcf6df27776b61a45dbbfab4dffffb8f16a6d270d40ee4b788e7f",
        "42c96d5216f2504c23091c45c6733f6e4fefccb0627edc0f3a57d892f7445e78")};
static const golden_document s3_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 1u, 4u, 1u, 2u, 164u, 8u,
        "bcda07eec3a4e18bfda26f71f5cdb87b4cd405585719c558f55fb53bea844582",
        "1af0c00cc5409cae6e671ad03b6a53f37f0320e146fe022f200938b13546beb6",
        "e7701cd040b2fcc0a8063a8e53f31232bb8c9739e8425920730ae4ccee0cd68f",
        "cc776c9c06b13f9397dd5370ac4419e08030f4a99526e86e8acecaeb3dfe49a9")};
static const golden_document s4_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 1u, 4u, 1u, 2u, 164u, 8u,
        "f4577b606b589a4eb10798ffcdf7428d211495370a3b247c6852094132cc7832",
        "9a865e4c80f46a040a2abcc1ae52f0ee1449c7378decad200e6a2cb42cb4afbc",
        "75266ae1be48c10bf68e2566d7811d81d62aad810df602b1aa1a2b696d8cec12",
        "e80dc86806ebbea66d825fb37859837c5e223bdca5ae7e3db57ffb21e70f43c3")};
static const golden_document s5_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 1u, 4u, 3u, 0u, 167u, 8u,
        "aa906dfa92c528251baea1b572a81356474b5f1d6966dae93db4868b759ed458",
        "03e17cb6580a4367b52bbc7a96fb080d5f52260ce9195c77eea4287ffe68c5d2",
        "6c7ae3968f08cce4d2ae6dea23558e3ea4dfb6e69512bd18179cadd974d1ece3",
        "b96f24907405caed74322e9714ada7312b63520b38e8edb673cc8b9dcd8a0015")};
static const golden_document s0_s2_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 1u, 3u, 2u, 0u, 83u, 6u,
        "dd8f2091d1d16564ce3d7512bbebbb2bfc8c3a4b5428095f63a14d4a1428b2db",
        "097813a3a02b43766e52ad9a10dbe9644702401161aa11ac2e9ce3838a311c32",
        "238b6943b7658790678abe8c5fd28c56444462c79a3894930bbc6b0276bbd175",
        "1b8cb7d2a1688f4cd63cf8bdcdae1e5f5caef7ac359b691932fa97efa1945b14"),
    GOLDEN_DOCUMENT(
        1u, 2u, 3u, 1u, 0u, 9u, 6u,
        "e30bab1bb290af8c4ff76810787ba585f1e2fb4c8eadb432393f81fe4048ff26",
        "277c96523e1e4d5f8b83d638d05c02d0ce0bb97a0fcdcc76996f8cd9e3b46a91",
        "0045ba0d46e0e5b3e55294505b8b87d7810b4857d3774c7356a0ddce8e6a169e",
        "c6e9089c199b97c8edc04a59ba574b8bca966b300eac8877474b91da5207a3f0")};

static const golden_case golden_cases[] = {
    {"S0", s0_sources, 1u, s0_documents, 1u, {1u, 1u, 3u, 2u, 0u, 83u, 6u},
     "be4ddc0a720523b16921c8e28b0da554a5fd59383f705ec41fa255c85daf17a8",
     "315c18341cd5fcfc1bed211848a711178a9e31854ab3beecd4c206d4e7a0ead7",
     "86483c6506d2537fa689c38d180e6de8dadabb826def622dcd646c09464aded6",
     false, false},
    {"S1", s1_sources, 1u, s1_documents, 1u, {1u, 1u, 3u, 2u, 0u, 83u, 6u},
     "be4ddc0a720523b16921c8e28b0da554a5fd59383f705ec41fa255c85daf17a8",
     "1faa7b53939a268b44d2bbdbc36dfd69eae3a2bafbb51210bd16d828541f5743",
     "7163066c07de8b1cc8590bcd8e8c87710052a6e037658f9f1d524aed92f6e768",
     false, false},
    {"S2", s2_sources, 1u, s2_documents, 1u, {1u, 2u, 3u, 1u, 0u, 9u, 6u},
     "3dc00d4575c198d4a5feeb10ed72e05b35cf09f5559de13cd3cfa65678037c0f",
     "cc2e5f65f7b08a4a9671b596e6d482a9df0fc8adf98471caf3fa7ab3d7925f28",
     "ce380878d9dc3bf3e45210c0331a27f3c2b3bdae63af4b71c3d92ba859f3380e",
     true, false},
    {"S3", s3_sources, 1u, s3_documents, 1u, {1u, 1u, 4u, 1u, 2u, 164u, 8u},
     "4beca56e4b367c61e574d86a5fc3b09237e0883cf86108d73373dce7e6e8cb6e",
     "6dae754f74186591a04555c07f421e9d25d0490ae6703cf9290b4126ff65124f",
     "d33286f9b7d3bb99eab2791f9a036a567a1b372619459aeb6e5fd51d6f68e60a",
     false, false},
    {"S4", s4_sources, 1u, s4_documents, 1u, {1u, 1u, 4u, 1u, 2u, 164u, 8u},
     "fd37e09e75e3a16e305802e4f0543fb9b29b14d37e96eea403badc4320cd4dca",
     "809261488427e51a6ed0d1aadc74c127ce6807364f0d0246a756df9f8b3fe875",
     "fd597d1daa188ae1bd9f5f20d025e634866799be210e03ecdbefa566477e8283",
     false, false},
    {"S5", s5_sources, 1u, s5_documents, 1u, {1u, 1u, 4u, 3u, 0u, 167u, 8u},
     "8467bdda421a4a493464b2a5152019859f0cadd576696c578fe65e38ad345147",
     "6dd496c691e420d071d90c1fbfdc25bc4228301b5087017c02e55397b1365475",
     "1e298e70676d3af678afa8bcfb6ccb6500ffe281f324c87ded8d2310cf0af40b",
     false, true},
    {"S0_S2", s0_s2_sources, 2u, s0_s2_documents, 2u,
     {2u, 3u, 6u, 3u, 0u, 92u, 12u},
     "901d0be98bf5986f920567f8f17715cc46c9dcbdb356a93f0dde62cb1ad49e7d",
     "696a8bfce72745a180ef5d465a9f2f435bb97874f5a20724ead029b2e686fedd",
     "b4deff60e2978bebe53c9c5b7b3b1f1d182575738bf799d24d9457c918f2f47d",
     false, false},
};

#undef GOLDEN_DOCUMENT

static w_seed_manifest_result measure_documents(
    const w_seed_manifest_source_input *sources, size_t source_count,
    uint64_t max_work_units, w_seed_manifest_counts *counts) {
  w_seed_manifest_input input;
  (void)memset(&input, 0, sizeof(input));
  input.documents = sources;
  input.document_count = source_count;
  input.limits = w_seed_manifest_default_limits();
  input.limits.max_work_units = max_work_units;
  input.scratch.name_slots = name_slots;
  input.scratch.name_slot_capacity = W_SEED_MANIFEST_MAX_STRUCTURAL_NODES;
  input.scratch.bytes = scalar_bytes;
  input.scratch.byte_capacity = sizeof(scalar_bytes);
  return w_seed_manifest_measure(&input, counts);
}

static w_seed_manifest_result measure(const uint8_t *bytes, size_t length,
                                      w_seed_manifest_counts *counts) {
  w_seed_manifest_source_input source;
  (void)memset(&source, 0, sizeof(source));
  source.bytes.data = bytes;
  source.bytes.length = length;
  return measure_documents(&source, 1u, W_SEED_MANIFEST_MAX_WORK_UNITS,
                           counts);
}

static bool golden_counts_match(const char *name,
                                w_seed_manifest_counts actual,
                                const uint32_t expected[7]) {
  const uint32_t values[7] = {actual.documents, actual.roots, actual.nodes,
                              actual.fields, actual.edges,
                              actual.canonical_bytes, actual.structural_nodes};
  if (memcmp(values, expected, sizeof(values)) == 0) return true;
  (void)fprintf(stderr, "manifest golden mismatch: %s.counts\n", name);
  return false;
}

static bool golden_digest_matches(const char *name, const char *field,
                                  const uint8_t actual[32],
                                  const char *expected) {
  char actual_text[65];
  for (size_t index = 0u; index < 32u; index += 1u) {
    static const char digits[] = "0123456789abcdef";
    actual_text[index * 2u] = digits[actual[index] >> 4u];
    actual_text[index * 2u + 1u] = digits[actual[index] & 0x0fu];
  }
  actual_text[64] = '\0';
  if (strlen(expected) == sizeof(actual_text) - 1u &&
      memcmp(actual_text, expected, sizeof(actual_text) - 1u) == 0)
    return true;
  (void)fprintf(stderr,
                "manifest golden mismatch: %s.%s expected=%s actual=%s\n",
                name, field, expected, actual_text);
  return false;
}

static void reset_golden_output(void) {
  (void)memset(output_documents, 0, sizeof(output_documents));
  (void)memset(output_roots, 0, sizeof(output_roots));
  (void)memset(output_nodes, 0, sizeof(output_nodes));
  (void)memset(output_fields, 0, sizeof(output_fields));
  (void)memset(output_edges, 0, sizeof(output_edges));
  (void)memset(output_canonical, 0, sizeof(output_canonical));
}

static bool read_golden_u32(const uint8_t *bytes, size_t length,
                            size_t *cursor, uint32_t *value) {
  if (bytes == NULL || cursor == NULL || value == NULL || *cursor > length ||
      length - *cursor < 4u)
    return false;
  *value = ((uint32_t)bytes[*cursor] << 24u) |
           ((uint32_t)bytes[*cursor + 1u] << 16u) |
           ((uint32_t)bytes[*cursor + 2u] << 8u) |
           (uint32_t)bytes[*cursor + 3u];
  *cursor += 4u;
  return true;
}

static bool read_golden_u64(const uint8_t *bytes, size_t length,
                            size_t *cursor, uint64_t *value) {
  if (bytes == NULL || cursor == NULL || value == NULL || *cursor > length ||
      length - *cursor < 8u)
    return false;
  uint64_t result = 0u;
  for (size_t index = 0u; index < 8u; index += 1u)
    result = (result << 8u) | bytes[*cursor + index];
  *value = result;
  *cursor += 8u;
  return true;
}

static bool golden_range_valid(uint32_t first, uint32_t count, size_t total) {
  return (size_t)first <= total && (size_t)count <= total - (size_t)first;
}

static bool golden_frame_matches(const uint8_t *bytes, size_t length,
                                 size_t *cursor, const char *tag,
                                 const uint8_t *payload, size_t payload_length) {
  uint32_t tag_length = 0u;
  uint64_t frame_length = 0u;
  const size_t expected_tag_length = strlen(tag);
  if (!read_golden_u32(bytes, length, cursor, &tag_length) ||
      tag_length != expected_tag_length || *cursor > length ||
      expected_tag_length > length - *cursor ||
      memcmp(bytes + *cursor, tag, expected_tag_length) != 0)
    return false;
  *cursor += expected_tag_length;
  if (!read_golden_u64(bytes, length, cursor, &frame_length) ||
      frame_length != payload_length || payload_length > length - *cursor ||
      (payload_length != 0u && payload == NULL) ||
      (payload_length != 0u &&
       memcmp(bytes + *cursor, payload, payload_length) != 0))
    return false;
  *cursor += payload_length;
  return true;
}

static bool golden_number_matches(const w_seed_manifest_program *program,
                                  const w_seed_manifest_node *node,
                                  uint8_t radix, const char *digits,
                                  const char *coefficient,
                                  const char *exponent) {
  if (program == NULL || node == NULL || node->canonical.offset == UINT32_MAX ||
      !golden_range_valid(node->canonical.offset, node->canonical.length,
                          program->canonical_byte_count))
    return false;
  const uint8_t *bytes = program->canonical_bytes + node->canonical.offset;
  const size_t length = node->canonical.length;
  const uint8_t *digit_bytes = (const uint8_t *)digits;
  const uint8_t *coefficient_bytes = (const uint8_t *)coefficient;
  const uint8_t *exponent_bytes = (const uint8_t *)exponent;
  size_t cursor = 0u;
  if (length == 0u || bytes[cursor++] != radix ||
      !golden_frame_matches(bytes, length, &cursor, "digits", digit_bytes,
                             strlen(digits)) ||
      !golden_frame_matches(bytes, length, &cursor, "coefficient",
                             coefficient_bytes, strlen(coefficient)) ||
      !golden_frame_matches(bytes, length, &cursor, "exponent", exponent_bytes,
                             strlen(exponent)) ||
      !golden_frame_matches(bytes, length, &cursor, "suffix", NULL, 0u))
    return false;
  return cursor == length;
}

static bool golden_field_node(const w_seed_manifest_program *program,
                              uint32_t document_index, const char *name,
                              uint32_t *node_index) {
  if (program == NULL || name == NULL || node_index == NULL ||
      document_index >= program->document_count || program->documents == NULL)
    return false;
  const w_seed_manifest_document *document = &program->documents[document_index];
  if (document->root_count == 0u || program->roots == NULL ||
      program->nodes == NULL || program->fields == NULL ||
      !golden_range_valid(document->first_root, document->root_count,
                          program->root_count))
    return false;
  const w_seed_manifest_root *root = &program->roots[document->first_root];
  if (root->record_node >= program->node_count) return false;
  const w_seed_manifest_node *record = &program->nodes[root->record_node];
  if (!golden_range_valid(record->first_child, record->child_count,
                          program->field_count))
    return false;
  const size_t name_length = strlen(name);
  for (uint32_t index = 0u; index < record->child_count; index += 1u) {
    const w_seed_manifest_field *field =
        &program->fields[record->first_child + index];
    if (field->name_span.end_byte - field->name_span.start_byte == name_length &&
        memcmp(document->source.data + field->name_span.start_byte, name,
               name_length) == 0) {
      *node_index = field->value_node;
      return true;
    }
  }
  return false;
}

static bool check_golden_s5(const w_seed_manifest_program *program) {
  uint32_t decimal_node = W_SEED_MANIFEST_NONE;
  uint32_t text_node = W_SEED_MANIFEST_NONE;
  uint32_t hex_node = W_SEED_MANIFEST_NONE;
  if (!golden_field_node(program, 0u, "decimal", &decimal_node) ||
      !golden_field_node(program, 0u, "text", &text_node) ||
      !golden_field_node(program, 0u, "hex", &hex_node) ||
      decimal_node >= program->node_count || text_node >= program->node_count ||
      hex_node >= program->node_count)
    return false;
  const w_seed_manifest_node *decimal = &program->nodes[decimal_node];
  const w_seed_manifest_node *text = &program->nodes[text_node];
  const w_seed_manifest_node *hex = &program->nodes[hex_node];
  if (decimal->kind != W_SEED_MANIFEST_NODE_NUMBER ||
      !golden_number_matches(program, decimal, 10u, "", "1", "5") ||
      text->kind != W_SEED_MANIFEST_NODE_STRING ||
      text->canonical.length != 3u ||
      memcmp(program->canonical_bytes + text->canonical.offset, "A\nB", 3u) !=
          0 ||
      hex->kind != W_SEED_MANIFEST_NODE_NUMBER ||
      !golden_number_matches(program, hex, 16u, "af", "", ""))
    return false;
  return true;
}

static bool run_golden_case(const golden_case *test, golden_observed *observed) {
  if (test == NULL || test->source_count > 2u ||
      test->document_count != test->source_count)
    return false;
  w_seed_manifest_source_input sources[2];
  (void)memset(sources, 0, sizeof(sources));
  for (size_t index = 0u; index < test->source_count; index += 1u) {
    sources[index].bytes.data = (const uint8_t *)test->sources[index];
    sources[index].bytes.length = strlen(test->sources[index]);
  }
  w_seed_manifest_counts counts;
  (void)memset(&counts, 0, sizeof(counts));
  w_seed_manifest_result measured =
      measure_documents(sources, test->source_count,
                        W_SEED_MANIFEST_MAX_WORK_UNITS, &counts);
  if (measured.status != W_SEED_MANIFEST_OK ||
      !golden_counts_match(test->name, counts, test->counts))
    return false;

  reset_golden_output();
  w_seed_manifest_input input;
  (void)memset(&input, 0, sizeof(input));
  input.documents = sources;
  input.document_count = test->source_count;
  input.limits = w_seed_manifest_default_limits();
  input.scratch.name_slots = name_slots;
  input.scratch.name_slot_capacity = W_SEED_MANIFEST_MAX_STRUCTURAL_NODES;
  input.scratch.bytes = scalar_bytes;
  input.scratch.byte_capacity = sizeof(scalar_bytes);
  w_seed_manifest_output output;
  (void)memset(&output, 0, sizeof(output));
  output.documents = output_documents;
  output.document_capacity = sizeof(output_documents) / sizeof(*output_documents);
  output.roots = output_roots;
  output.root_capacity = sizeof(output_roots) / sizeof(*output_roots);
  output.nodes = output_nodes;
  output.node_capacity = sizeof(output_nodes) / sizeof(*output_nodes);
  output.fields = output_fields;
  output.field_capacity = sizeof(output_fields) / sizeof(*output_fields);
  output.edges = output_edges;
  output.edge_capacity = sizeof(output_edges) / sizeof(*output_edges);
  output.canonical_bytes = output_canonical;
  output.canonical_byte_capacity = sizeof(output_canonical);
  w_seed_manifest_result result = w_seed_manifest_run(&input, &output);
  if (result.status != W_SEED_MANIFEST_OK ||
      !same_counts(result.required, counts) ||
      !same_counts(result.written, counts)) {
    (void)fprintf(stderr, "manifest golden mismatch: %s.run\n", test->name);
    return false;
  }
  w_seed_manifest_program program;
  w_seed_manifest_scratch verify_scratch = {
      name_slots, W_SEED_MANIFEST_MAX_STRUCTURAL_NODES, scalar_bytes,
      sizeof(scalar_bytes)};
  if (!w_seed_manifest_program_from_output(&output, &result, &program) ||
      !w_seed_manifest_verify(&program, &result, &verify_scratch)) {
    (void)fprintf(stderr, "manifest golden mismatch: %s.verify\n", test->name);
    return false;
  }
  for (size_t index = 0u; index < test->document_count; index += 1u) {
    const golden_document *expected = &test->documents[index];
    const w_seed_manifest_document *actual = &output.documents[index];
    if (!golden_counts_match(test->name, actual->counts, expected->counts) ||
        !golden_digest_matches(test->name, "document.source",
                               actual->source_digest, expected->source) ||
        !golden_digest_matches(test->name, "document.semantic",
                               actual->semantic_digest, expected->semantic) ||
        !golden_digest_matches(test->name, "document.provenance",
                               actual->provenance_digest,
                               expected->provenance) ||
        !golden_digest_matches(test->name, "document.receipt",
                               actual->receipt_digest, expected->receipt))
      return false;
  }
  if (!golden_digest_matches(test->name, "batch.semantic",
                             result.semantic_digest, test->semantic) ||
      !golden_digest_matches(test->name, "batch.provenance",
                             result.provenance_digest, test->provenance) ||
      !golden_digest_matches(test->name, "batch.receipt", result.receipt_digest,
                             test->receipt))
    return false;
  if (test->check_roots &&
      (output.roots[0].kind != W_SEED_MANIFEST_ROOT_PACKAGE ||
       output.roots[0].ordinal != 0u ||
       output.roots[1].kind != W_SEED_MANIFEST_ROOT_BUILD ||
       output.roots[1].ordinal != 1u))
    return false;
  if (test->check_s5 && !check_golden_s5(&program)) return false;
  if (observed != NULL) {
    observed->result = result;
    for (size_t index = 0u; index < test->document_count; index += 1u) {
      (void)memcpy(observed->source[index], output.documents[index].source_digest,
                   W_SEED_MANIFEST_DIGEST_BYTES);
      (void)memcpy(observed->semantic[index],
                   output.documents[index].semantic_digest,
                   W_SEED_MANIFEST_DIGEST_BYTES);
      (void)memcpy(observed->provenance[index],
                   output.documents[index].provenance_digest,
                   W_SEED_MANIFEST_DIGEST_BYTES);
      (void)memcpy(observed->receipt[index], output.documents[index].receipt_digest,
                   W_SEED_MANIFEST_DIGEST_BYTES);
    }
  }
  return true;
}

typedef enum {
  MANIFEST_MUTATE_RESULT_DIGEST = 0,
  MANIFEST_MUTATE_DOCUMENT_RECEIPT,
  MANIFEST_MUTATE_RESULT_COUNT,
  MANIFEST_MUTATE_NODE_RANGE,
  MANIFEST_MUTATE_FIELD_ORDER,
  MANIFEST_MUTATE_FIELD_OWNER,
  MANIFEST_MUTATE_NODE_KIND,
  MANIFEST_MUTATE_BOOL,
  MANIFEST_MUTATE_CANONICAL,
} manifest_mutation;

static bool reject_manifest_mutation(manifest_mutation mutation,
                                     w_seed_manifest_program *program,
                                     w_seed_manifest_result *result,
                                     const w_seed_manifest_scratch *scratch) {
  if (program == NULL || result == NULL || scratch == NULL) return false;
  w_seed_manifest_node *nodes = (w_seed_manifest_node *)program->nodes;
  w_seed_manifest_field *fields = (w_seed_manifest_field *)program->fields;
  uint8_t *canonical_bytes = (uint8_t *)program->canonical_bytes;
  bool accepted = true;
  switch (mutation) {
    case MANIFEST_MUTATE_RESULT_DIGEST: {
      const uint8_t saved = result->semantic_digest[0];
      result->semantic_digest[0] ^= 1u;
      accepted = w_seed_manifest_verify(program, result, scratch);
      result->semantic_digest[0] = saved;
      break;
    }
    case MANIFEST_MUTATE_DOCUMENT_RECEIPT: {
      w_seed_manifest_document *document =
          (w_seed_manifest_document *)program->documents;
      const uint8_t saved = document[0].receipt_digest[0];
      document[0].receipt_digest[0] ^= 1u;
      accepted = w_seed_manifest_verify(program, result, scratch);
      document[0].receipt_digest[0] = saved;
      break;
    }
    case MANIFEST_MUTATE_RESULT_COUNT: {
      const uint32_t saved = result->written.nodes;
      result->written.nodes += 1u;
      accepted = w_seed_manifest_verify(program, result, scratch);
      result->written.nodes = saved;
      break;
    }
    case MANIFEST_MUTATE_NODE_RANGE: {
      const w_seed_span saved = nodes[0].source_span;
      nodes[0].source_span.end_byte += 1u;
      accepted = w_seed_manifest_verify(program, result, scratch);
      nodes[0].source_span = saved;
      break;
    }
    case MANIFEST_MUTATE_FIELD_ORDER: {
      const uint32_t saved = fields[0].ordinal;
      fields[0].ordinal ^= 1u;
      accepted = w_seed_manifest_verify(program, result, scratch);
      fields[0].ordinal = saved;
      break;
    }
    case MANIFEST_MUTATE_FIELD_OWNER: {
      const uint32_t saved = fields[0].owner_record;
      fields[0].owner_record = W_SEED_MANIFEST_NONE;
      accepted = w_seed_manifest_verify(program, result, scratch);
      fields[0].owner_record = saved;
      break;
    }
    case MANIFEST_MUTATE_NODE_KIND: {
      uint32_t index = W_SEED_MANIFEST_NONE;
      for (uint32_t candidate = 0u; candidate < program->node_count;
           candidate += 1u)
        if (nodes[candidate].kind == W_SEED_MANIFEST_NODE_STRING) {
          index = candidate;
          break;
        }
      if (index == W_SEED_MANIFEST_NONE) return false;
      const w_seed_manifest_node_kind saved = nodes[index].kind;
      nodes[index].kind = W_SEED_MANIFEST_NODE_BOOL;
      accepted = w_seed_manifest_verify(program, result, scratch);
      nodes[index].kind = saved;
      break;
    }
    case MANIFEST_MUTATE_BOOL: {
      uint32_t index = W_SEED_MANIFEST_NONE;
      for (uint32_t candidate = 0u; candidate < program->node_count;
           candidate += 1u)
        if (nodes[candidate].kind == W_SEED_MANIFEST_NODE_BOOL) {
          index = candidate;
          break;
        }
      if (index == W_SEED_MANIFEST_NONE) return false;
      const bool saved = nodes[index].boolean_value;
      nodes[index].boolean_value = !saved;
      accepted = w_seed_manifest_verify(program, result, scratch);
      nodes[index].boolean_value = saved;
      break;
    }
    case MANIFEST_MUTATE_CANONICAL: {
      if (program->canonical_byte_count == 0u ||
          program->canonical_bytes == NULL)
        return false;
      const uint8_t saved = canonical_bytes[0];
      canonical_bytes[0] ^= 1u;
      accepted = w_seed_manifest_verify(program, result, scratch);
      canonical_bytes[0] = saved;
      break;
    }
    default:
      return false;
  }
  return !accepted;
}

static void snapshot_manifest_output(void) {
  (void)memcpy(output_snapshot_documents, output_documents,
               sizeof(output_documents));
  (void)memcpy(output_snapshot_roots, output_roots, sizeof(output_roots));
  (void)memcpy(output_snapshot_nodes, output_nodes, sizeof(output_nodes));
  (void)memcpy(output_snapshot_fields, output_fields, sizeof(output_fields));
  (void)memcpy(output_snapshot_edges, output_edges, sizeof(output_edges));
  (void)memcpy(output_snapshot_canonical, output_canonical,
               sizeof(output_canonical));
}

static bool manifest_output_is_unchanged(void) {
  return memcmp(output_snapshot_documents, output_documents,
                sizeof(output_documents)) == 0 &&
         memcmp(output_snapshot_roots, output_roots, sizeof(output_roots)) == 0 &&
         memcmp(output_snapshot_nodes, output_nodes, sizeof(output_nodes)) == 0 &&
         memcmp(output_snapshot_fields, output_fields, sizeof(output_fields)) == 0 &&
         memcmp(output_snapshot_edges, output_edges, sizeof(output_edges)) == 0 &&
         memcmp(output_snapshot_canonical, output_canonical,
                sizeof(output_canonical)) == 0;
}

static bool reject_root_source(const uint8_t *bytes, size_t length,
                               w_seed_manifest_status status,
                               w_seed_manifest_error_kind error) {
  const w_seed_manifest_counts sentinel = {
      UINT32_C(0xa5a5a5a5), UINT32_C(0xa5a5a5a5), UINT32_C(0xa5a5a5a5),
      UINT32_C(0xa5a5a5a5), UINT32_C(0xa5a5a5a5), UINT32_C(0xa5a5a5a5),
      UINT32_C(0xa5a5a5a5)};
  w_seed_manifest_counts counts = sentinel;
  const w_seed_manifest_result result = measure(bytes, length, &counts);
  return result.status == status && result.error == error &&
         same_counts(counts, sentinel);
}

static bool check_package_build_roots(void) {
  static const uint8_t multiple[] =
      "package { name: \"first\" }\n"
      "build { schema: \"w.build/1\" }\n"
      "package { name: \"second\" }\n";
  static const uint8_t multiple_without_build[] =
      "package {}\npackage {}\n";
  static const uint8_t duplicate_build[] =
      "package {}\n"
      "build { schema: \"w.build/1\" }\n"
      "build { schema: \"w.build/1\" }\n";
  static const uint8_t missing_build_schema[] =
      "package {}\nbuild { selected: \"app\" }\n";
  static const uint8_t wrong_build_schema[] =
      "package {}\nbuild { schema: \"w.build/2\" }\n";
  static const uint8_t nonstring_build_schema[] =
      "package {}\nbuild { schema: .wBuild }\n";
  static const uint8_t nested_build_schema[] =
      "package {}\nbuild { nested: { schema: \"w.build/1\" } }\n";
  static const uint8_t build_without_package[] =
      "build { schema: \"w.build/1\" }\n";
  static const uint8_t legacy_workspace[] =
      "package {}\nworkspace {}\n";
  w_seed_manifest_counts counts;

  if (!reject_root_source(multiple_without_build,
                          sizeof(multiple_without_build) - 1u,
                          W_SEED_MANIFEST_SYNTAX,
                          W_SEED_MANIFEST_ERROR_BUILD_REQUIRED) ||
      !reject_root_source(duplicate_build, sizeof(duplicate_build) - 1u,
                          W_SEED_MANIFEST_DUPLICATE,
                          W_SEED_MANIFEST_ERROR_ROOT_DUPLICATE) ||
      !reject_root_source(missing_build_schema,
                          sizeof(missing_build_schema) - 1u,
                          W_SEED_MANIFEST_SYNTAX,
                          W_SEED_MANIFEST_ERROR_BUILD_SCHEMA_REQUIRED) ||
      !reject_root_source(wrong_build_schema,
                          sizeof(wrong_build_schema) - 1u,
                          W_SEED_MANIFEST_SYNTAX,
                          W_SEED_MANIFEST_ERROR_BUILD_SCHEMA_INVALID) ||
      !reject_root_source(nonstring_build_schema,
                          sizeof(nonstring_build_schema) - 1u,
                          W_SEED_MANIFEST_SYNTAX,
                          W_SEED_MANIFEST_ERROR_BUILD_SCHEMA_INVALID) ||
      !reject_root_source(nested_build_schema,
                          sizeof(nested_build_schema) - 1u,
                          W_SEED_MANIFEST_SYNTAX,
                          W_SEED_MANIFEST_ERROR_BUILD_SCHEMA_REQUIRED) ||
      !reject_root_source(build_without_package,
                          sizeof(build_without_package) - 1u,
                          W_SEED_MANIFEST_SYNTAX,
                          W_SEED_MANIFEST_ERROR_ROOT_REQUIRED) ||
      !reject_root_source(legacy_workspace, sizeof(legacy_workspace) - 1u,
                          W_SEED_MANIFEST_SYNTAX,
                          W_SEED_MANIFEST_ERROR_ROOT_INVALID))
    return false;

  (void)memset(&counts, 0, sizeof(counts));
  w_seed_manifest_result result =
      measure(multiple, sizeof(multiple) - 1u, &counts);
  if (result.status != W_SEED_MANIFEST_OK || counts.roots != 3u)
    return false;

  w_seed_manifest_source_input source;
  (void)memset(&source, 0, sizeof(source));
  source.bytes.data = multiple;
  source.bytes.length = sizeof(multiple) - 1u;
  w_seed_manifest_input input;
  (void)memset(&input, 0, sizeof(input));
  input.documents = &source;
  input.document_count = 1u;
  input.limits = w_seed_manifest_default_limits();
  input.scratch = (w_seed_manifest_scratch){
      name_slots, W_SEED_MANIFEST_MAX_STRUCTURAL_NODES, scalar_bytes,
      sizeof(scalar_bytes)};
  w_seed_manifest_output output = {
      .documents = output_documents,
      .document_capacity = sizeof(output_documents) / sizeof(*output_documents),
      .roots = output_roots,
      .root_capacity = sizeof(output_roots) / sizeof(*output_roots),
      .nodes = output_nodes,
      .node_capacity = sizeof(output_nodes) / sizeof(*output_nodes),
      .fields = output_fields,
      .field_capacity = sizeof(output_fields) / sizeof(*output_fields),
      .edges = output_edges,
      .edge_capacity = sizeof(output_edges) / sizeof(*output_edges),
      .canonical_bytes = output_canonical,
      .canonical_byte_capacity = sizeof(output_canonical),
  };
  result = w_seed_manifest_run(&input, &output);
  if (result.status != W_SEED_MANIFEST_OK ||
      result.required.roots != 3u ||
      output_roots[0].kind != W_SEED_MANIFEST_ROOT_PACKAGE ||
      output_roots[1].kind != W_SEED_MANIFEST_ROOT_PACKAGE ||
      output_roots[2].kind != W_SEED_MANIFEST_ROOT_BUILD ||
      output_roots[0].ordinal != 0u || output_roots[1].ordinal != 1u ||
      output_roots[2].ordinal != 2u ||
      !(output_roots[0].keyword_span.start_byte <
            output_roots[2].keyword_span.start_byte &&
        output_roots[2].keyword_span.start_byte <
            output_roots[1].keyword_span.start_byte))
    return false;
  w_seed_manifest_program program;
  w_seed_manifest_scratch verify_scratch = input.scratch;
  if (!w_seed_manifest_program_from_output(&output, &result, &program) ||
      !w_seed_manifest_verify(&program, &result, &verify_scratch))
    return false;

  snapshot_manifest_output();
  input.limits.max_roots_per_document = 2u;
  const w_seed_manifest_counts count_sentinel = {
      UINT32_C(0xa5a5a5a5), UINT32_C(0xa5a5a5a5), UINT32_C(0xa5a5a5a5),
      UINT32_C(0xa5a5a5a5), UINT32_C(0xa5a5a5a5), UINT32_C(0xa5a5a5a5),
      UINT32_C(0xa5a5a5a5)};
  w_seed_manifest_counts count_after_failure = count_sentinel;
  result = w_seed_manifest_measure(&input, &count_after_failure);
  if (result.status != W_SEED_MANIFEST_LIMIT ||
      result.error != W_SEED_MANIFEST_ERROR_ROOT_LIMIT ||
      !same_counts(count_after_failure, count_sentinel) ||
      !manifest_output_is_unchanged())
    return false;

  source.bytes.data = multiple_without_build;
  source.bytes.length = sizeof(multiple_without_build) - 1u;
  input.limits = w_seed_manifest_default_limits();
  result = w_seed_manifest_run(&input, &output);
  return result.status == W_SEED_MANIFEST_SYNTAX &&
         result.error == W_SEED_MANIFEST_ERROR_BUILD_REQUIRED &&
         manifest_output_is_unchanged();
}

static bool check_manifest_capacity_alias(
    const w_seed_manifest_input *input, const w_seed_manifest_output *output,
    w_seed_manifest_counts counts) {
  if (input == NULL || output == NULL || counts.nodes == 0u || counts.fields == 0u)
    return false;
  snapshot_manifest_output();
  w_seed_manifest_output variant = *output;
  variant.node_capacity = counts.nodes - 1u;
  w_seed_manifest_result result = w_seed_manifest_run(input, &variant);
  if (result.status != W_SEED_MANIFEST_CAPACITY ||
      !manifest_output_is_unchanged())
    return false;

  variant = *output;
  variant.root_capacity = counts.roots - 1u;
  result = w_seed_manifest_run(input, &variant);
  if (result.status != W_SEED_MANIFEST_CAPACITY ||
      !manifest_output_is_unchanged())
    return false;

  variant = *output;
  variant.fields = NULL;
  result = w_seed_manifest_run(input, &variant);
  if (result.status != W_SEED_MANIFEST_CAPACITY ||
      !manifest_output_is_unchanged())
    return false;

  variant = *output;
  variant.roots = (w_seed_manifest_root *)variant.nodes;
  result = w_seed_manifest_run(input, &variant);
  if (result.status != W_SEED_MANIFEST_ALIAS ||
      !manifest_output_is_unchanged())
    return false;

  w_seed_manifest_counts count_sentinel;
  (void)memset(&count_sentinel, 0xa5, sizeof(count_sentinel));
  w_seed_manifest_input input_variant = *input;
  input_variant.scratch.bytes = NULL;
  input_variant.scratch.byte_capacity = 0u;
  result = w_seed_manifest_measure(&input_variant, &count_sentinel);
  if (result.status != W_SEED_MANIFEST_CAPACITY ||
      count_sentinel.documents != 0xa5a5a5a5u)
    return false;

  (void)memset(&count_sentinel, 0xa5, sizeof(count_sentinel));
  input_variant = *input;
  input_variant.scratch.bytes = (uint8_t *)input->documents[0].bytes.data;
  result = w_seed_manifest_measure(&input_variant, &count_sentinel);
  return result.status == W_SEED_MANIFEST_ALIAS &&
         count_sentinel.documents == 0xa5a5a5a5u;
}

static int measure_file(const char *relative) {
  char path[1024];
  const int written = snprintf(path, sizeof(path), "%s/%s",
                               W_SEED_REPOSITORY_ROOT, relative);
  if (written <= 0 || (size_t)written >= sizeof(path)) return EXIT_FAILURE;
  FILE *file = fopen(path, "rb");
  if (file == NULL) return EXIT_FAILURE;
  const size_t length = fread(source_bytes, 1u, sizeof(source_bytes), file);
  if (ferror(file) != 0 || fclose(file) != 0 || length == sizeof(source_bytes))
    return EXIT_FAILURE;
  w_seed_manifest_counts counts;
  (void)memset(&counts, 0, sizeof(counts));
  w_seed_manifest_result result = measure(source_bytes, length, &counts);
  if (result.status != W_SEED_MANIFEST_OK) {
    (void)fprintf(stderr, "%s: status=%d error=%d byte=%" PRIuMAX "\n",
                  relative, (int)result.status, (int)result.error,
                  (uintmax_t)result.byte_offset);
    return EXIT_FAILURE;
  }
  w_seed_manifest_input input;
  (void)memset(&input, 0, sizeof(input));
  w_seed_manifest_source_input source;
  (void)memset(&source, 0, sizeof(source));
  source.bytes.data = source_bytes;
  source.bytes.length = length;
  input.documents = &source;
  input.document_count = 1u;
  input.limits = w_seed_manifest_default_limits();
  input.scratch.name_slots = name_slots;
  input.scratch.name_slot_capacity = W_SEED_MANIFEST_MAX_STRUCTURAL_NODES;
  input.scratch.bytes = scalar_bytes;
  input.scratch.byte_capacity = sizeof(scalar_bytes);
  w_seed_manifest_output output;
  (void)memset(&output, 0, sizeof(output));
  output.document_capacity = counts.documents;
  output.root_capacity = counts.roots;
  output.node_capacity = counts.nodes;
  output.field_capacity = counts.fields;
  output.edge_capacity = counts.edges;
  output.canonical_byte_capacity = counts.canonical_bytes;
  output.documents = (w_seed_manifest_document *)calloc(
      output.document_capacity, sizeof(*output.documents));
  output.roots = (w_seed_manifest_root *)calloc(output.root_capacity,
                                                 sizeof(*output.roots));
  output.nodes = (w_seed_manifest_node *)calloc(output.node_capacity,
                                                sizeof(*output.nodes));
  output.fields = (w_seed_manifest_field *)calloc(output.field_capacity,
                                                  sizeof(*output.fields));
  output.edges = (w_seed_manifest_edge *)calloc(output.edge_capacity,
                                                sizeof(*output.edges));
  output.canonical_bytes = (uint8_t *)calloc(output.canonical_byte_capacity, 1u);
  if (output.documents == NULL || output.roots == NULL || output.nodes == NULL ||
      (output.field_capacity != 0u && output.fields == NULL) ||
      (output.edge_capacity != 0u && output.edges == NULL) ||
      (output.canonical_byte_capacity != 0u && output.canonical_bytes == NULL)) {
    free(output.documents);
    free(output.roots);
    free(output.nodes);
    free(output.fields);
    free(output.edges);
    free(output.canonical_bytes);
    return EXIT_FAILURE;
  }
  result = w_seed_manifest_run(&input, &output);
  int outcome = EXIT_SUCCESS;
  if (result.status != W_SEED_MANIFEST_OK) {
    (void)fprintf(stderr, "%s: run status=%d error=%d\n", relative,
                  (int)result.status, (int)result.error);
    outcome = EXIT_FAILURE;
  } else {
    w_seed_manifest_program program;
    w_seed_manifest_scratch verify_scratch = {
        name_slots, W_SEED_MANIFEST_MAX_STRUCTURAL_NODES, scalar_bytes,
        sizeof(scalar_bytes)};
    if (!w_seed_manifest_program_from_output(&output, &result, &program) ||
        !w_seed_manifest_verify(&program, &result, &verify_scratch)) {
      (void)fprintf(stderr, "%s: program/verify failed\n", relative);
      outcome = EXIT_FAILURE;
    }
  }
  free(output.documents);
  free(output.roots);
  free(output.nodes);
  free(output.fields);
  free(output.edges);
  free(output.canonical_bytes);
  return outcome;
}

typedef struct {
  const uint8_t *sources[2];
  size_t lengths[2];
} guarded_test_context;

typedef enum {
  GUARDED_TEST_STABLE = 0,
  GUARDED_TEST_FIRST_CAPACITY,
  GUARDED_TEST_PROGRAM_SENTINEL,
  GUARDED_TEST_MISALIGNED_ROOT,
  GUARDED_TEST_COPIED_GUARD,
  GUARDED_TEST_STALE_GENERATION,
  GUARDED_TEST_CROSS_CONTEXT,
  GUARDED_TEST_ALIAS,
  GUARDED_TEST_SECOND_BYTES,
  GUARDED_TEST_SECOND_BINDING,
  GUARDED_TEST_SECOND_LIMIT,
  GUARDED_TEST_SECOND_MALFORMED,
} guarded_test_mode;

static guarded_test_mode guarded_test_mode_value;
static size_t guarded_test_read_calls;
static size_t guarded_test_revalidate_calls;
static bool guarded_test_misaligned_backend;

static w_seed_owner_guard_backend_result guarded_test_owner_begin(
    void *context_value, w_seed_byte_view source_path,
    w_seed_owner_guard_observation *observations, size_t observation_capacity) {
  (void)context_value;
  if (source_path.length != sizeof("source") - 1u ||
      memcmp(source_path.data, "source", source_path.length) != 0 ||
      observation_capacity < 2u)
    return (w_seed_owner_guard_backend_result){
        W_SEED_OWNER_GUARD_BACKEND_INVALID,
        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, 0u, 0u, 0u};
  observations[0] = (w_seed_owner_guard_observation){0u, 0u, false};
  observations[1] = (w_seed_owner_guard_observation){1u, 1u, true};
  return (w_seed_owner_guard_backend_result){
      W_SEED_OWNER_GUARD_BACKEND_OK,
      W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT,
      1u, 0u, 41u, 2u, 2u};
}

static w_seed_owner_guard_backend_result guarded_test_owner_revalidate(
    void *context_value, uint64_t generation,
    w_seed_owner_guard_observation *observations, size_t observation_capacity) {
  (void)context_value;
  guarded_test_revalidate_calls += 1u;
  if (generation != 41u || observation_capacity < 2u)
    return (w_seed_owner_guard_backend_result){
        W_SEED_OWNER_GUARD_BACKEND_INVALID,
        W_SEED_OWNER_GUARD_BACKEND_PHASE_VALIDATE,
        W_SEED_OWNER_GUARD_NO_LEVEL, 0u, generation, 0u, 0u};
  observations[0] = (w_seed_owner_guard_observation){0u, 0u, false};
  observations[1] = (w_seed_owner_guard_observation){1u, 1u, true};
  return (w_seed_owner_guard_backend_result){
      W_SEED_OWNER_GUARD_BACKEND_OK,
      W_SEED_OWNER_GUARD_BACKEND_PHASE_COMMIT,
      1u, 0u, generation, 2u, 2u};
}

static void guarded_test_owner_abort(void *context_value) {
  (void)context_value;
}

static void guarded_test_owner_destroy(void *context_value, uint64_t generation) {
  (void)context_value;
  (void)generation;
}

static w_seed_manifest_backend_result guarded_test_backend_result(
    w_seed_manifest_backend_status status,
    w_seed_manifest_backend_phase phase, uint64_t generation,
    w_seed_owner_guard_candidate_ref candidate, size_t byte_count,
    size_t required) {
  return (w_seed_manifest_backend_result){
      status, phase, generation, candidate, byte_count, required, {0}, {0}, {0}};
}

static w_seed_manifest_backend_result guarded_test_read(
    const void *context_value, uint64_t generation,
    w_seed_owner_guard_candidate_ref candidate, uint8_t *bytes,
    size_t byte_capacity, size_t byte_limit) {
  const guarded_test_context *context =
      (const guarded_test_context *)context_value;
  const size_t index = candidate.candidate_index;
  guarded_test_read_calls += 1u;
  if (index >= 2u || generation != 41u)
    return guarded_test_backend_result(
        W_SEED_MANIFEST_BACKEND_INVALID,
        W_SEED_MANIFEST_BACKEND_PHASE_VALIDATE, generation, candidate, 0u, 0u);
  const bool second = (guarded_test_read_calls > 2u);
  if (second && guarded_test_mode_value == GUARDED_TEST_SECOND_LIMIT)
    return guarded_test_backend_result(
        W_SEED_MANIFEST_BACKEND_LIMIT,
        W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF, generation, candidate, 0u,
        byte_limit + 1u);
  if (second && guarded_test_mode_value == GUARDED_TEST_SECOND_MALFORMED)
    return guarded_test_backend_result(
        W_SEED_MANIFEST_BACKEND_NOT_CALLED,
        W_SEED_MANIFEST_BACKEND_PHASE_NONE, generation, candidate, 0u, 0u);
  const uint8_t *source = context->sources[index];
  size_t length = context->lengths[index];
  if (second && guarded_test_mode_value == GUARDED_TEST_SECOND_BYTES &&
      length != 0u)
    length -= 1u;
  if (length > byte_capacity)
    return guarded_test_backend_result(
        W_SEED_MANIFEST_BACKEND_CAPACITY,
        W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF, generation, candidate, 0u,
        context->lengths[index]);
  (void)memcpy(bytes, source, length);
  w_seed_manifest_backend_result result = guarded_test_backend_result(
      W_SEED_MANIFEST_BACKEND_OK, W_SEED_MANIFEST_BACKEND_PHASE_CLOSE,
      generation, candidate, length, length);
  test_source_digest(bytes, length, result.source_digest);
  result.context_binding[0] = 0x11u;
  result.candidate_binding[0] = (uint8_t)(0x20u + index);
  if (second && guarded_test_mode_value == GUARDED_TEST_SECOND_BINDING)
    result.candidate_binding[0] ^= 0x01u;
  return result;
}

static int run_guarded_fixture(guarded_test_mode mode,
                               w_seed_manifest_status expected_status) {
  static const uint8_t source_a[] = "package { a: 1 }\n";
  static const uint8_t source_b[] = "package { b: true }\n";
  guarded_test_context context = {
      {source_a, source_b}, {sizeof(source_a) - 1u, sizeof(source_b) - 1u}};
  guarded_test_context other_context = context;
  w_seed_owner_guard_observation owner_staged[2];
  w_seed_owner_guard_observation owner_revalidation[2];
  w_seed_owner_guard_candidate_ref owner_candidates[2];
  w_seed_owner_guard_backend owner_backend = {
      &context, guarded_test_owner_begin,
      guarded_test_owner_revalidate, guarded_test_owner_abort,
      guarded_test_owner_destroy};
  w_seed_owner_guard_input owner_input = {
      {(const uint8_t *)"source", sizeof("source") - 1u}, 2u,
      {owner_staged, 2u, owner_revalidation, 2u, owner_candidates, 2u},
      owner_backend, sizeof(context)};
  w_seed_owner_guard guard;
  (void)memset(&guard, 0, sizeof(guard));
  w_seed_owner_guard_result owner_result;
  (void)memset(&owner_result, 0, sizeof(owner_result));
  CHECK(w_seed_owner_guard_begin(&owner_input, &guard, &owner_result) ==
        W_SEED_OWNER_GUARD_OK);

  uint8_t first[2][128];
  uint8_t second[2][128];
  w_seed_manifest_read_slot slots[2] = {
      {first[0], sizeof(first[0]), second[0], sizeof(second[0])},
      {first[1], sizeof(first[1]), second[1], sizeof(second[1])}};
  if (mode == GUARDED_TEST_FIRST_CAPACITY) slots[0].first_capacity = 4u;
  if (mode == GUARDED_TEST_ALIAS) slots[1].first_bytes = slots[0].first_bytes;
  w_seed_manifest_source_input staged_sources[2];
  w_seed_manifest_document staged_documents[2];
  w_seed_manifest_root staged_roots[4];
  w_seed_manifest_node staged_nodes[16];
  w_seed_manifest_field staged_fields[16];
  w_seed_manifest_edge staged_edges[8];
  uint8_t staged_canonical[256];
  w_seed_manifest_document published_documents[2];
  w_seed_manifest_root published_roots[4];
  w_seed_manifest_node published_nodes[16];
  w_seed_manifest_field published_fields[16];
  w_seed_manifest_edge published_edges[8];
  uint8_t published_canonical[256];
  (void)memset(staged_sources, 0, sizeof(staged_sources));
  (void)memset(staged_documents, 0, sizeof(staged_documents));
  (void)memset(published_documents, 0xa5, sizeof(published_documents));
  (void)memset(published_roots, 0xa5, sizeof(published_roots));
  (void)memset(published_nodes, 0xa5, sizeof(published_nodes));
  (void)memset(published_fields, 0xa5, sizeof(published_fields));
  (void)memset(published_edges, 0xa5, sizeof(published_edges));
  (void)memset(published_canonical, 0xa5, sizeof(published_canonical));
  const w_seed_manifest_output staged = {
      staged_documents, 2u, staged_roots, 4u, staged_nodes, 16u,
      staged_fields, 16u, staged_edges, 8u, staged_canonical,
      sizeof(staged_canonical)};
  const w_seed_manifest_output published = {
      published_documents, 2u, published_roots, 4u, published_nodes, 16u,
      published_fields, 16u, published_edges, 8u, published_canonical,
      sizeof(published_canonical)};
  w_seed_manifest_backend manifest_backend = {
      NULL, &guard, &context, sizeof(context), 41u,
      guarded_test_read};
  manifest_backend.owner = &manifest_backend;
  w_seed_manifest_guarded_input guarded_input = {
      &guard, &manifest_backend, w_seed_manifest_default_limits(),
      {slots, 2u, staged_sources, 2u,
       {name_slots, W_SEED_MANIFEST_MAX_STRUCTURAL_NODES, scalar_bytes,
        sizeof(scalar_bytes)},
       staged, published}};
  w_seed_owner_guard copied_guard;
  if (mode == GUARDED_TEST_COPIED_GUARD) {
    copied_guard = guard;
    guarded_input.guard = &copied_guard;
  }
  if (mode == GUARDED_TEST_STALE_GENERATION)
    manifest_backend.generation = 40u;
  if (mode == GUARDED_TEST_CROSS_CONTEXT) {
    manifest_backend.context = &other_context;
    manifest_backend.context_size = sizeof(other_context);
  }
  uint8_t misaligned_guard_storage[sizeof(w_seed_owner_guard) + 1u];
  uint8_t misaligned_backend_storage[sizeof(w_seed_manifest_backend) + 1u];
  if (mode == GUARDED_TEST_MISALIGNED_ROOT) {
    if (guarded_test_misaligned_backend) {
      (void)memcpy(misaligned_backend_storage + 1u, &manifest_backend,
                   sizeof(manifest_backend));
      guarded_input.backend = (w_seed_manifest_backend *)(void *)
          (misaligned_backend_storage + 1u);
    } else {
      (void)memcpy(misaligned_guard_storage + 1u, &guard,
                   sizeof(guard));
      guarded_input.guard = (w_seed_owner_guard *)(void *)
          (misaligned_guard_storage + 1u);
    }
  }
  w_seed_manifest_program program;
  (void)memset(&program,
               (mode == GUARDED_TEST_PROGRAM_SENTINEL ||
                mode == GUARDED_TEST_MISALIGNED_ROOT) ? 0xa5 : 0,
               sizeof(program));
  const w_seed_manifest_program program_snapshot = program;
  uint8_t published_snapshot_documents[sizeof(published_documents)];
  uint8_t published_snapshot_roots[sizeof(published_roots)];
  uint8_t published_snapshot_nodes[sizeof(published_nodes)];
  uint8_t published_snapshot_fields[sizeof(published_fields)];
  uint8_t published_snapshot_edges[sizeof(published_edges)];
  uint8_t published_snapshot_canonical[sizeof(published_canonical)];
  (void)memcpy(published_snapshot_documents, published_documents,
               sizeof(published_documents));
  (void)memcpy(published_snapshot_roots, published_roots,
               sizeof(published_roots));
  (void)memcpy(published_snapshot_nodes, published_nodes,
               sizeof(published_nodes));
  (void)memcpy(published_snapshot_fields, published_fields,
               sizeof(published_fields));
  (void)memcpy(published_snapshot_edges, published_edges,
               sizeof(published_edges));
  (void)memcpy(published_snapshot_canonical, published_canonical,
               sizeof(published_canonical));
  guarded_test_mode_value = mode;
  guarded_test_read_calls = 0u;
  guarded_test_revalidate_calls = 0u;
  const w_seed_manifest_result result =
      w_seed_manifest_guarded_run(&guarded_input, &program);
  CHECK(result.status == expected_status);
  if (expected_status == W_SEED_MANIFEST_OK) {
    CHECK(result.phase == W_SEED_MANIFEST_PHASE_COMMIT);
    CHECK(result.owner_guard_revalidate_called);
    CHECK(guarded_test_read_calls == 4u);
    CHECK(guarded_test_revalidate_calls == 1u);
    CHECK(program.document_count == 2u);
    CHECK(program.documents[0].source.data == second[0]);
    CHECK(program.documents[1].source.data == second[1]);
    CHECK(program.documents[0].candidate.candidate_index == 0u);
    CHECK(program.documents[1].candidate.candidate_index == 1u);
    CHECK(program.documents[0].binding_kind ==
          W_SEED_MANIFEST_BINDING_OWNER_GUARD);
  } else {
    CHECK(guarded_test_read_calls <= 4u);
    if (mode == GUARDED_TEST_SECOND_LIMIT ||
        mode == GUARDED_TEST_SECOND_BINDING ||
        mode == GUARDED_TEST_SECOND_BYTES ||
        mode == GUARDED_TEST_SECOND_MALFORMED)
      CHECK(guarded_test_revalidate_calls == 1u);
    else CHECK(guarded_test_revalidate_calls == 0u);
    CHECK(memcmp(&program, &program_snapshot, sizeof(program)) == 0);
    CHECK(memcmp(published_documents, published_snapshot_documents,
                 sizeof(published_documents)) == 0);
    CHECK(memcmp(published_roots, published_snapshot_roots,
                 sizeof(published_roots)) == 0);
    CHECK(memcmp(published_nodes, published_snapshot_nodes,
                 sizeof(published_nodes)) == 0);
    CHECK(memcmp(published_fields, published_snapshot_fields,
                 sizeof(published_fields)) == 0);
    CHECK(memcmp(published_edges, published_snapshot_edges,
                 sizeof(published_edges)) == 0);
    CHECK(memcmp(published_canonical, published_snapshot_canonical,
                 sizeof(published_canonical)) == 0);
  }
  w_seed_owner_guard_destroy(&guard);
  return EXIT_SUCCESS;
}

static int run_guarded_tests(void) {
  CHECK(run_guarded_fixture(GUARDED_TEST_STABLE, W_SEED_MANIFEST_OK) ==
        EXIT_SUCCESS);
  CHECK(run_guarded_fixture(GUARDED_TEST_PROGRAM_SENTINEL,
                            W_SEED_MANIFEST_OK) == EXIT_SUCCESS);
  guarded_test_misaligned_backend = false;
  CHECK(run_guarded_fixture(GUARDED_TEST_MISALIGNED_ROOT,
                            W_SEED_MANIFEST_INVALID) == EXIT_SUCCESS);
  guarded_test_misaligned_backend = true;
  CHECK(run_guarded_fixture(GUARDED_TEST_MISALIGNED_ROOT,
                            W_SEED_MANIFEST_INVALID) == EXIT_SUCCESS);
  CHECK(run_guarded_fixture(GUARDED_TEST_FIRST_CAPACITY,
                            W_SEED_MANIFEST_CAPACITY) == EXIT_SUCCESS);
  CHECK(run_guarded_fixture(GUARDED_TEST_COPIED_GUARD,
                            W_SEED_MANIFEST_STALE) == EXIT_SUCCESS);
  CHECK(run_guarded_fixture(GUARDED_TEST_STALE_GENERATION,
                            W_SEED_MANIFEST_STALE) == EXIT_SUCCESS);
  CHECK(run_guarded_fixture(GUARDED_TEST_CROSS_CONTEXT,
                            W_SEED_MANIFEST_STALE) == EXIT_SUCCESS);
  CHECK(run_guarded_fixture(GUARDED_TEST_ALIAS, W_SEED_MANIFEST_ALIAS) ==
        EXIT_SUCCESS);
  CHECK(run_guarded_fixture(GUARDED_TEST_SECOND_BYTES,
                            W_SEED_MANIFEST_MUTATED) == EXIT_SUCCESS);
  CHECK(run_guarded_fixture(GUARDED_TEST_SECOND_BINDING,
                            W_SEED_MANIFEST_MUTATED) == EXIT_SUCCESS);
  CHECK(run_guarded_fixture(GUARDED_TEST_SECOND_LIMIT,
                            W_SEED_MANIFEST_MUTATED) == EXIT_SUCCESS);
  CHECK(run_guarded_fixture(GUARDED_TEST_SECOND_MALFORMED,
                            W_SEED_MANIFEST_FAULT) == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}

static int run_manifest_tests(void) {
  static const uint8_t small[] = "package { alpha: 1 beta: \"A\" }\n";
  w_seed_manifest_counts counts;
  (void)memset(&counts, 0, sizeof(counts));
  w_seed_manifest_result result = measure(small, sizeof(small) - 1u, &counts);
  CHECK(result.status == W_SEED_MANIFEST_OK);
  CHECK(result.phase == W_SEED_MANIFEST_PHASE_MEASURE);
  CHECK(counts.documents == 1u && counts.roots == 1u && counts.fields == 2u);
  CHECK(counts.canonical_bytes == 83u);

  static const uint8_t large_exponent[] =
      "package { value: 1e99999999999999999999 }\n";
  (void)memset(&counts, 0, sizeof(counts));
  result = measure(large_exponent, sizeof(large_exponent) - 1u, &counts);
  CHECK(result.status == W_SEED_MANIFEST_OK);

  static const uint8_t quantity[] = "package { value: 1<m / s^2> }\n";
  (void)memset(&counts, 0, sizeof(counts));
  result = measure(quantity, sizeof(quantity) - 1u, &counts);
  CHECK(result.status == W_SEED_MANIFEST_OK);

  static const uint8_t invalid_quantity[] = "package { value: 1<++> }\n";
  result = measure(invalid_quantity, sizeof(invalid_quantity) - 1u, &counts);
  CHECK(result.status == W_SEED_MANIFEST_SYNTAX);

  static const uint8_t empty_constructor[] = "package { value: .item() }\n";
  (void)memset(&counts, 0xa5, sizeof(counts));
  result = measure(empty_constructor, sizeof(empty_constructor) - 1u, &counts);
  CHECK(result.status == W_SEED_MANIFEST_SYNTAX);
  CHECK(result.error == W_SEED_MANIFEST_ERROR_VALUE_REQUIRED);

  static const uint8_t work_source[] = "package { value: 1 }\n";
  w_seed_manifest_source_input work_input;
  (void)memset(&work_input, 0, sizeof(work_input));
  work_input.bytes.data = work_source;
  work_input.bytes.length = sizeof(work_source) - 1u;
  result = measure_documents(&work_input, 1u, 327u, &counts);
  CHECK(result.status == W_SEED_MANIFEST_OK);
  result = measure_documents(&work_input, 1u, 326u, &counts);
  CHECK(result.status == W_SEED_MANIFEST_LIMIT);
  CHECK(result.error == W_SEED_MANIFEST_ERROR_WORK_LIMIT);

  static const uint8_t batch_source_a[] = "package {}\n";
  static const uint8_t batch_source_b[] = "package {}\n";
  w_seed_manifest_source_input batch[2];
  (void)memset(batch, 0, sizeof(batch));
  batch[0].bytes.data = batch_source_a;
  batch[0].bytes.length = sizeof(batch_source_a) - 1u;
  batch[1].bytes.data = batch_source_b;
  batch[1].bytes.length = sizeof(batch_source_b) - 1u;
  result = measure_documents(batch, 1u, 50u, &counts);
  CHECK(result.status == W_SEED_MANIFEST_OK);
  result = measure_documents(batch, 2u, 50u, &counts);
  CHECK(result.status == W_SEED_MANIFEST_LIMIT);
  CHECK(result.error == W_SEED_MANIFEST_ERROR_WORK_LIMIT);

  CHECK(check_package_build_roots());

  CHECK(measure_file("reference/last-light/build.w") == EXIT_SUCCESS);
  CHECK(measure_file("reference/syntax-atlas/build.w") == EXIT_SUCCESS);

  golden_observed golden_results[sizeof(golden_cases) / sizeof(*golden_cases)];
  (void)memset(golden_results, 0, sizeof(golden_results));
  for (size_t index = 0u; index < sizeof(golden_cases) / sizeof(*golden_cases);
       index += 1u)
    CHECK(run_golden_case(&golden_cases[index], &golden_results[index]));
  CHECK(memcmp(golden_results[0].semantic[0], golden_results[1].semantic[0],
               W_SEED_MANIFEST_DIGEST_BYTES) == 0);
  CHECK(memcmp(golden_results[0].source[0], golden_results[1].source[0],
               W_SEED_MANIFEST_DIGEST_BYTES) != 0);
  CHECK(memcmp(golden_results[0].provenance[0], golden_results[1].provenance[0],
               W_SEED_MANIFEST_DIGEST_BYTES) != 0);
  CHECK(memcmp(golden_results[0].receipt[0], golden_results[1].receipt[0],
               W_SEED_MANIFEST_DIGEST_BYTES) != 0);
  CHECK(memcmp(golden_results[3].semantic[0], golden_results[4].semantic[0],
               W_SEED_MANIFEST_DIGEST_BYTES) != 0);
  CHECK(memcmp(golden_results[6].provenance[1], golden_results[2].provenance[0],
               W_SEED_MANIFEST_DIGEST_BYTES) != 0);
  CHECK(memcmp(golden_results[6].receipt[1], golden_results[2].receipt[0],
               W_SEED_MANIFEST_DIGEST_BYTES) != 0);

  static const uint8_t run_source[] =
      "build { schema: \"w.build/1\", z: [1, 2], a: .Foo(label: \"x\", true), "
      "b: 1.00e+2, c: 1KiB, d: 1<m / s> }\n"
      "package { name: \"w\\n\" }\n";
  w_seed_manifest_source_input run_input_source;
  (void)memset(&run_input_source, 0, sizeof(run_input_source));
  run_input_source.bytes.data = run_source;
  run_input_source.bytes.length = sizeof(run_source) - 1u;
  w_seed_manifest_input run_input;
  (void)memset(&run_input, 0, sizeof(run_input));
  run_input.documents = &run_input_source;
  run_input.document_count = 1u;
  run_input.limits = w_seed_manifest_default_limits();
  run_input.scratch.name_slots = name_slots;
  run_input.scratch.name_slot_capacity =
      W_SEED_MANIFEST_MAX_STRUCTURAL_NODES;
  run_input.scratch.bytes = scalar_bytes;
  run_input.scratch.byte_capacity = sizeof(scalar_bytes);
  w_seed_manifest_output run_output;
  (void)memset(&run_output, 0, sizeof(run_output));
  run_output.documents = output_documents;
  run_output.document_capacity = 2u;
  run_output.roots = output_roots;
  run_output.root_capacity = 4u;
  run_output.nodes = output_nodes;
  run_output.node_capacity = 64u;
  run_output.fields = output_fields;
  run_output.field_capacity = 64u;
  run_output.edges = output_edges;
  run_output.edge_capacity = 64u;
  run_output.canonical_bytes = output_canonical;
  run_output.canonical_byte_capacity = sizeof(output_canonical);
  result = w_seed_manifest_run(&run_input, &run_output);
  CHECK(result.status == W_SEED_MANIFEST_OK);
  CHECK(result.phase == W_SEED_MANIFEST_PHASE_RUN);
  CHECK(result.required.documents == 1u &&
        same_counts(result.required, result.written));
  w_seed_manifest_program run_program;
  CHECK(w_seed_manifest_program_from_output(&run_output, &result,
                                            &run_program));
  w_seed_manifest_scratch verify_scratch;
  verify_scratch.name_slots = name_slots;
  verify_scratch.name_slot_capacity = W_SEED_MANIFEST_MAX_STRUCTURAL_NODES;
  verify_scratch.bytes = scalar_bytes;
  verify_scratch.byte_capacity = sizeof(scalar_bytes);
  CHECK(w_seed_manifest_verify(&run_program, &result, &verify_scratch));
  static const manifest_mutation mutations[] = {
      MANIFEST_MUTATE_RESULT_DIGEST,
      MANIFEST_MUTATE_DOCUMENT_RECEIPT,
      MANIFEST_MUTATE_RESULT_COUNT,
      MANIFEST_MUTATE_NODE_RANGE,
      MANIFEST_MUTATE_FIELD_ORDER,
      MANIFEST_MUTATE_FIELD_OWNER,
      MANIFEST_MUTATE_NODE_KIND,
      MANIFEST_MUTATE_BOOL,
      MANIFEST_MUTATE_CANONICAL,
  };
  for (size_t index = 0u; index < sizeof(mutations) / sizeof(*mutations);
       index += 1u)
    CHECK(reject_manifest_mutation(mutations[index], &run_program, &result,
                                   &verify_scratch));
  CHECK(check_manifest_capacity_alias(&run_input, &run_output, result.required));
  CHECK(run_guarded_tests() == EXIT_SUCCESS);
  return EXIT_SUCCESS;
}

int main(void) {
  const int status = run_manifest_tests();
  return status == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
