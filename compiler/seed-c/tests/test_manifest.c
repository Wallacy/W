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
static const char s2_source[] = "workspace {}\npackage {}\n";
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
        "2f44e64c86b98866777b0b559195b2662a753ff3787aa2fa460ed38b216d3c65",
        "794fb70a3a6c6b67981431ba97602c183071d0f0a3b661e9f744dd7f950dafe8",
        "1d8ea6e2b4f239b0c4a3e6d4f33378b3a2077804bfe07a436ab6d5c36fb3a5e2",
        "77d7817c026699591dc0f95085c8148a47fc817902e598ebdec5e2e2e16b267f")};
static const golden_document s1_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 1u, 3u, 2u, 0u, 83u, 6u,
        "19ae808cc6125fdd950b8d7c2e21718b45dfc45967f630188ade71b453d3960f",
        "794fb70a3a6c6b67981431ba97602c183071d0f0a3b661e9f744dd7f950dafe8",
        "70f368a8b423bf89373704b5089a423d0b555f9aa67621caf7bdf639feb24890",
        "ebd7200cfb8415df2a5ca100fbb32ae978160c09945f551d61a0b7195756bdf9")};
static const golden_document s2_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 2u, 2u, 0u, 0u, 0u, 4u,
        "b9f315ca23902dd7b05ca6403cbcd59d75c247faeb8401a19bcfc30e573a41df",
        "4687b3fcc2441603c4e8c5f1f4b7eaccb28dc7471eadde3fa53b3f3e702108f3",
        "9e734d08c7dc79397dd80e37b1179fc80edf544348272d0300f51cb3fe8a9c4b",
        "42490fc9fe98a33a091806c0d33a9c7e0641d2946bd75488cb6f5b0084c8e8bb")};
static const golden_document s3_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 1u, 4u, 1u, 2u, 164u, 8u,
        "8dd1b489e2cbd9b8f121c233893550d27568e9733da2b03bf62df5bcb49b7365",
        "76cbe9c7f37bc7651838412766b9c434d5ad47de2f3eac0591665fb2b45fc13b",
        "31887b27010a9f3bc650c90c2658a8bebf6ee8b8e4c32f848c21dce9da7fab0c",
        "fcc173c43b45dee44962fae0263c85736ae52ab78f65988426e362a28cc2bd5e")};
static const golden_document s4_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 1u, 4u, 1u, 2u, 164u, 8u,
        "b1be81c1f0dee36adb4a8c811eac24039191daea0b5feee913535780e53f257b",
        "c276161e074c8e13e3b041a707277267383ac13f016e084129232631b6650369",
        "8d58ad7103e8162092c5a973ffb15f281d0db56fb42f8d4b5962c9c6897eb936",
        "90d8fe47d40e6ce7dd70b0dcfe15b5c4b01523c3db7e387de715454f80bde0b1")};
static const golden_document s5_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 1u, 4u, 3u, 0u, 167u, 8u,
        "b1e9677b41597272e2d2c1be41a56dfc3e564a5d8fa94fc0d9ba48a1d5ab1566",
        "1b165953f56bca327d0a5f6f869f29e21342f972b9610a0eba9374733042532b",
        "587dacd5b2cab5af18710f844e482128724f7d54470770504dc8bee8036d9863",
        "7707f85ae159071d502561532cb58b5ec81b0eb4a2c83b419fb05925cbeb49e7")};
static const golden_document s0_s2_documents[] = {
    GOLDEN_DOCUMENT(
        1u, 1u, 3u, 2u, 0u, 83u, 6u,
        "2f44e64c86b98866777b0b559195b2662a753ff3787aa2fa460ed38b216d3c65",
        "794fb70a3a6c6b67981431ba97602c183071d0f0a3b661e9f744dd7f950dafe8",
        "1d8ea6e2b4f239b0c4a3e6d4f33378b3a2077804bfe07a436ab6d5c36fb3a5e2",
        "77d7817c026699591dc0f95085c8148a47fc817902e598ebdec5e2e2e16b267f"),
    GOLDEN_DOCUMENT(
        1u, 2u, 2u, 0u, 0u, 0u, 4u,
        "b9f315ca23902dd7b05ca6403cbcd59d75c247faeb8401a19bcfc30e573a41df",
        "4687b3fcc2441603c4e8c5f1f4b7eaccb28dc7471eadde3fa53b3f3e702108f3",
        "51f93588d10a443993a74b09ac47ef54b9403a751cd1a263f114530a0cd68bae",
        "96496b00b45715baadc1a46306518266ea66f8f059f8c6de2435560f8edeba33")};

static const golden_case golden_cases[] = {
    {"S0", s0_sources, 1u, s0_documents, 1u, {1u, 1u, 3u, 2u, 0u, 83u, 6u},
     "0e5d2fb4aa0a26251911f7f85f7b38220086196b9dc333368cc78dbdf004a158",
     "727d3e8f832a58e93f3c9162423b9a743a501a6cf610683828c93a85a930ea69",
     "e919595832fed9265bb1711b6f160f160ec28453873008ab34ed7c2bbcdf9f8b",
     false, false},
    {"S1", s1_sources, 1u, s1_documents, 1u, {1u, 1u, 3u, 2u, 0u, 83u, 6u},
     "0e5d2fb4aa0a26251911f7f85f7b38220086196b9dc333368cc78dbdf004a158",
     "f2d44c482d3e96ebd292124704c040efa4f824bb32c4fcb87464c5590c55a493",
     "eb3ebc846ce3e1780a35da2108ecb446e867dfd3d03c233646b52d2f59660b5d",
     false, false},
    {"S2", s2_sources, 1u, s2_documents, 1u, {1u, 2u, 2u, 0u, 0u, 0u, 4u},
     "5f0d26525af8630fcec1e3bf5c28d2fb44c63c4ad9e374612bf5565ecbd1fc1f",
     "e7b735f4d7d70bd48906690c637450078613e567119e58323081f77ba64128a7",
     "2d4171087f491c60385844110c1e3b37603998b43df8c29acc87d775a0bba6cc",
     true, false},
    {"S3", s3_sources, 1u, s3_documents, 1u, {1u, 1u, 4u, 1u, 2u, 164u, 8u},
     "f46953e0ecf67065c54faeb51c4828e8f61e711b0e957636147bc8fa16312e8c",
     "0ca41ef4593ce2b7282e607e60a9f537c5c138ddf2cdfafe8a735d293af6cd22",
     "5af7847274dacd86bfe955b5bb9c09f0d6f9dfcf4dc2dc3057ef710f09699011",
     false, false},
    {"S4", s4_sources, 1u, s4_documents, 1u, {1u, 1u, 4u, 1u, 2u, 164u, 8u},
     "62a4c84da39d46bd174e1aba5fc9fe0df944f742d157bea799ed30e91a3186a5",
     "19aba5274f2fcac04b5cb75ef2dc211c93cfc2d3d8eb242699a1927e9a300cc5",
     "d5548ccf6fa53807c875a1488cb1cf7580586fa4adf8f92d15e8a7310df85782",
     false, false},
    {"S5", s5_sources, 1u, s5_documents, 1u, {1u, 1u, 4u, 3u, 0u, 167u, 8u},
     "1de20a742bd30d97805f15a9f7a4b0fb336af91f7adc075828466f5ed577a972",
     "b66b5fb677a2d5f374ec05081b3fc6a6d7b1a98ccaf5accbc6151ea3535c8cae",
     "c29d9cfb250dc24d2d805e993602474c380ce0b4025aadc8ed1bd7271fe7b06c",
     false, true},
    {"S0_S2", s0_s2_sources, 2u, s0_s2_documents, 2u,
     {2u, 3u, 5u, 2u, 0u, 83u, 10u},
     "377c9ec12efa15549f8bc71bd1fe3aa4968d9693f7d11577722283d6242b1fed",
     "b2c87d21c6ad20075b79b8337afb60d94bab3cb92dd022c852d5c831332a8b0b",
     "f216ce3ea77a4f4a2cc050a2c36fd6d901cf5d7c6f16e95daa9ee9c08a6518ca",
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
       output.roots[1].kind != W_SEED_MANIFEST_ROOT_WORKSPACE ||
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
  static const uint8_t source_b[] = "workspace { b: true }\n";
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

int main(void) {
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

  CHECK(measure_file("reference/last-light/build.w") == EXIT_SUCCESS);
  CHECK(measure_file("reference/last-light/packages/menu-compiler/build.w") ==
        EXIT_SUCCESS);
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
      "workspace { z: [1, 2], a: .Foo(label: \"x\", true), "
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
