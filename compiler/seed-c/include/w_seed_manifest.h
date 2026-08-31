#ifndef W_SEED_MANIFEST_H
#define W_SEED_MANIFEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_owner_guard.h"
#include "w_seed_source.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MAN0 is an internal structural reader for data-only build.w documents. It
 * does not select an owner or workspace, validate a manifest schema, resolve
 * imports, acquire modules, or authorize execution. */
#define W_SEED_MANIFEST_SCHEMA_VERSION "w-seed-man0-1"
#define W_SEED_MANIFEST_DOCUMENT_SOURCE_TAG \
  "w.seed.man0.document.source/1"
#define W_SEED_MANIFEST_DOCUMENT_SEMANTIC_TAG \
  "w.seed.man0.document.semantic/1"
#define W_SEED_MANIFEST_DOCUMENT_PROVENANCE_TAG \
  "w.seed.man0.document.provenance/1"
#define W_SEED_MANIFEST_DOCUMENT_RECEIPT_TAG \
  "w.seed.man0.document.receipt/1"
#define W_SEED_MANIFEST_BATCH_SEMANTIC_TAG \
  "w.seed.man0.batch.semantic/1"
#define W_SEED_MANIFEST_BATCH_PROVENANCE_TAG \
  "w.seed.man0.batch.provenance/1"
#define W_SEED_MANIFEST_BATCH_RECEIPT_TAG \
  "w.seed.man0.batch.receipt/1"
#define W_SEED_MANIFEST_CONTEXT_TAG "w.seed.man0.context/1"
#define W_SEED_MANIFEST_CANDIDATE_TAG "w.seed.man0.candidate/1"
#define W_SEED_MANIFEST_DIGEST_BYTES 32u
#define W_SEED_MANIFEST_NONE UINT32_MAX
#define W_SEED_MANIFEST_MAX_DOCUMENT_BYTES (1u * 1024u * 1024u)
#define W_SEED_MANIFEST_MAX_AGGREGATE_BYTES (16u * 1024u * 1024u)
#define W_SEED_MANIFEST_MAX_NESTING 256u
#define W_SEED_MANIFEST_MAX_STRUCTURAL_NODES 262144u
#define W_SEED_MANIFEST_MAX_ROOTS_PER_DOCUMENT 2u
#define W_SEED_MANIFEST_MAX_DOCUMENTS W_SEED_OWNER_GUARD_MAX_LEVELS
#define W_SEED_MANIFEST_MAX_SCALAR_SOURCE_BYTES (1u * 1024u * 1024u)
#define W_SEED_MANIFEST_MAX_NUMBER_DIGITS (1u * 1024u * 1024u)
#define W_SEED_MANIFEST_MAX_DECODED_SCALAR_BYTES (1u * 1024u * 1024u)
#define W_SEED_MANIFEST_MAX_CANONICAL_BYTES (16u * 1024u * 1024u)
#define W_SEED_MANIFEST_MAX_WORK_UNITS UINT64_C(67108864)
#define W_SEED_MANIFEST_SCALAR_SCRATCH_OVERHEAD 256u
#define W_SEED_MANIFEST_NO_BYTE SIZE_MAX

typedef enum {
  W_SEED_MANIFEST_OK = 0,
  W_SEED_MANIFEST_INVALID,
  W_SEED_MANIFEST_ALIAS,
  W_SEED_MANIFEST_CAPACITY,
  W_SEED_MANIFEST_LIMIT,
  W_SEED_MANIFEST_UTF8,
  W_SEED_MANIFEST_BOM,
  W_SEED_MANIFEST_SYNTAX,
  W_SEED_MANIFEST_DUPLICATE,
  W_SEED_MANIFEST_MUTATED,
  W_SEED_MANIFEST_BOUNDARY,
  W_SEED_MANIFEST_REPARSE,
  W_SEED_MANIFEST_UNSUPPORTED,
  W_SEED_MANIFEST_IO,
  W_SEED_MANIFEST_STALE,
  W_SEED_MANIFEST_FAULT,
} w_seed_manifest_status;

typedef enum {
  W_SEED_MANIFEST_PHASE_NONE = 0,
  W_SEED_MANIFEST_PHASE_VALIDATE,
  W_SEED_MANIFEST_PHASE_READ_FIRST,
  W_SEED_MANIFEST_PHASE_REVALIDATE_OWNER_GUARD,
  W_SEED_MANIFEST_PHASE_READ_SECOND,
  W_SEED_MANIFEST_PHASE_COMPARE_WAVES,
  W_SEED_MANIFEST_PHASE_MEASURE,
  W_SEED_MANIFEST_PHASE_RUN,
  W_SEED_MANIFEST_PHASE_VERIFY,
  W_SEED_MANIFEST_PHASE_COMMIT,
} w_seed_manifest_phase;

typedef enum {
  W_SEED_MANIFEST_ERROR_NONE = 0,
  W_SEED_MANIFEST_ERROR_SOURCE_EMPTY,
  W_SEED_MANIFEST_ERROR_SOURCE_TOO_LARGE,
  W_SEED_MANIFEST_ERROR_AGGREGATE_TOO_LARGE,
  W_SEED_MANIFEST_ERROR_DOCUMENT_LIMIT,
  W_SEED_MANIFEST_ERROR_INVALID_UTF8,
  W_SEED_MANIFEST_ERROR_UTF8_BOM,
  W_SEED_MANIFEST_ERROR_INVALID_TOKEN,
  W_SEED_MANIFEST_ERROR_UNTERMINATED_COMMENT,
  W_SEED_MANIFEST_ERROR_UNTERMINATED_STRING,
  W_SEED_MANIFEST_ERROR_INVALID_STRING_ESCAPE,
  W_SEED_MANIFEST_ERROR_INTERPOLATION,
  W_SEED_MANIFEST_ERROR_EXECUTABLE_FORM,
  W_SEED_MANIFEST_ERROR_ROOT_REQUIRED,
  W_SEED_MANIFEST_ERROR_ROOT_INVALID,
  W_SEED_MANIFEST_ERROR_ROOT_DUPLICATE,
  W_SEED_MANIFEST_ERROR_ROOT_LIMIT,
  W_SEED_MANIFEST_ERROR_FIELD_REQUIRED,
  W_SEED_MANIFEST_ERROR_FIELD_DUPLICATE,
  W_SEED_MANIFEST_ERROR_COLON_REQUIRED,
  W_SEED_MANIFEST_ERROR_VALUE_REQUIRED,
  W_SEED_MANIFEST_ERROR_COMMA_REQUIRED,
  W_SEED_MANIFEST_ERROR_CONSTRUCTOR_LABEL_DUPLICATE,
  W_SEED_MANIFEST_ERROR_NESTING_LIMIT,
  W_SEED_MANIFEST_ERROR_NODE_LIMIT,
  W_SEED_MANIFEST_ERROR_SCALAR_SOURCE_LIMIT,
  W_SEED_MANIFEST_ERROR_NUMBER_DIGIT_LIMIT,
  W_SEED_MANIFEST_ERROR_DECODED_SCALAR_LIMIT,
  W_SEED_MANIFEST_ERROR_CANONICAL_LIMIT,
  W_SEED_MANIFEST_ERROR_WORK_LIMIT,
  W_SEED_MANIFEST_ERROR_TRAILING_SOURCE,
} w_seed_manifest_error_kind;

/* Every effective limit is nonzero and no greater than its MAN0 ceiling.
 * Callers can lower any value. structural_nodes counts roots, value nodes,
 * fields, list items, and constructor arguments together. Each scanned byte,
 * compared byte, scalar digit operation, and sort comparison charges work. */
typedef struct {
  uint32_t max_document_bytes;
  uint32_t max_aggregate_bytes;
  uint32_t max_nesting;
  uint32_t max_structural_nodes;
  uint32_t max_roots_per_document;
  uint32_t max_documents;
  uint32_t max_scalar_source_bytes;
  uint32_t max_number_digits;
  uint32_t max_decoded_scalar_bytes;
  uint32_t max_canonical_bytes;
  uint64_t max_work_units;
} w_seed_manifest_limits;

typedef enum {
  W_SEED_MANIFEST_BINDING_NONE = 0,
  W_SEED_MANIFEST_BINDING_OWNER_GUARD,
} w_seed_manifest_binding_kind;

/* Public pure measure/run accept only NONE with every following binding byte
 * and integer canonically zero. Guarded run constructs OWNER_GUARD inputs only
 * after validating the live guard and opaque backend binding. */
typedef struct {
  w_seed_byte_view bytes;
  w_seed_manifest_binding_kind binding_kind;
  uint64_t generation;
  w_seed_owner_guard_candidate_ref candidate;
  uint8_t context_binding[W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t candidate_binding[W_SEED_MANIFEST_DIGEST_BYTES];
} w_seed_manifest_source_input;

/* The parser clears and reuses this caller-owned scratch on every measure, run,
 * or verify. Name slots retain bounded field/label permutations and value spans;
 * they are not wire records. Required name_slot_capacity is effective
 * max_structural_nodes. Required byte_capacity is effective
 * max_decoded_scalar_bytes plus SCALAR_SCRATCH_OVERHEAD. A large numeric token
 * is rejected at max_number_digits before normalization begins. */
typedef struct {
  uint64_t hash;
  uint64_t scope;
  w_seed_span name_span;
  w_seed_span value_span;
  uint32_t source_ordinal;
  bool has_name;
  bool occupied;
} w_seed_manifest_name_slot;

typedef struct {
  w_seed_manifest_name_slot *name_slots;
  size_t name_slot_capacity;
  uint8_t *bytes;
  size_t byte_capacity;
} w_seed_manifest_scratch;

typedef struct {
  const w_seed_manifest_source_input *documents;
  size_t document_count;
  w_seed_manifest_limits limits;
  w_seed_manifest_scratch scratch;
} w_seed_manifest_input;

typedef struct {
  uint32_t offset;
  uint32_t length;
} w_seed_manifest_canonical_bytes;

typedef enum {
  W_SEED_MANIFEST_ROOT_PACKAGE = 0,
  W_SEED_MANIFEST_ROOT_WORKSPACE,
} w_seed_manifest_root_kind;

typedef enum {
  W_SEED_MANIFEST_NODE_RECORD = 0,
  W_SEED_MANIFEST_NODE_LIST,
  W_SEED_MANIFEST_NODE_CONSTRUCTOR,
  W_SEED_MANIFEST_NODE_MEMBER,
  W_SEED_MANIFEST_NODE_STRING,
  W_SEED_MANIFEST_NODE_NUMBER,
  W_SEED_MANIFEST_NODE_SIZE,
  W_SEED_MANIFEST_NODE_QUANTITY,
  W_SEED_MANIFEST_NODE_BOOL,
} w_seed_manifest_node_kind;

typedef enum {
  W_SEED_MANIFEST_EDGE_LIST_ITEM = 0,
  W_SEED_MANIFEST_EDGE_CONSTRUCTOR_ARGUMENT,
} w_seed_manifest_edge_kind;

/* Source spans are exact half-open byte ranges in document.source. Optional
 * commas and surrounding trivia are not part of field or edge spans. An absent
 * span is {NO_BYTE, NO_BYTE}; an empty present span has equal in-range ends.
 * Roots are grouped by document and ordered PACKAGE then WORKSPACE. */
typedef struct {
  uint32_t document_index;
  uint32_t ordinal;
  w_seed_manifest_root_kind kind;
  uint32_t record_node;
  w_seed_span keyword_span;
  w_seed_span source_span;
} w_seed_manifest_root;

typedef struct {
  w_seed_manifest_node_kind kind;
  uint32_t document_index;
  uint32_t parent_node;
  /* Dense physical ordinal among values of the parent. A root record uses its
   * physical root ordinal and has parent_node=NONE. */
  uint32_t source_ordinal;
  w_seed_span source_span;
  /* MEMBER and CONSTRUCTOR use a present identifier span. Other kinds use
   * {NO_BYTE, NO_BYTE}. */
  w_seed_span name_span;
  /* RECORD uses the canonically ordered field range. LIST and CONSTRUCTOR use
   * the source-ordered edge range. Other kinds require NONE and zero. */
  uint32_t first_child;
  uint32_t child_count;
  /* STRING, NUMBER, SIZE, and QUANTITY use decoded or normalized canonical
   * bytes. Other kinds require {NONE, 0}. Canonical bytes can contain NUL and
   * have no terminator. */
  w_seed_manifest_canonical_bytes canonical;
  bool boolean_value;
} w_seed_manifest_node;

typedef struct {
  uint32_t owner_record;
  /* Dense canonical ordinal by UTF-8 name bytes inside owner_record. */
  uint32_t ordinal;
  uint32_t value_node;
  w_seed_span name_span;
  w_seed_span source_span;
} w_seed_manifest_field;

typedef struct {
  w_seed_manifest_edge_kind kind;
  uint32_t owner_node;
  /* Dense physical ordinal inside owner_node. */
  uint32_t ordinal;
  uint32_t value_node;
  bool has_label;
  w_seed_span label_span;
  w_seed_span source_span;
} w_seed_manifest_edge;

typedef struct {
  uint32_t documents;
  uint32_t roots;
  uint32_t nodes;
  uint32_t fields;
  uint32_t edges;
  uint32_t canonical_bytes;
  uint32_t structural_nodes;
} w_seed_manifest_counts;

typedef struct {
  /* Pure run borrows input bytes. Guarded run points this view at the matching
   * read_slots[document_index].second_bytes buffer promoted at commit. */
  w_seed_byte_view source;
  w_seed_manifest_binding_kind binding_kind;
  uint64_t generation;
  w_seed_owner_guard_candidate_ref candidate;
  uint32_t first_root;
  uint32_t root_count;
  uint32_t first_node;
  uint32_t node_count;
  uint32_t first_field;
  uint32_t field_count;
  uint32_t first_edge;
  uint32_t edge_count;
  uint32_t first_canonical_byte;
  uint32_t canonical_byte_count;
  w_seed_manifest_counts counts;
  uint8_t context_binding[W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t candidate_binding[W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t source_digest[W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t semantic_digest[W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t provenance_digest[W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t receipt_digest[W_SEED_MANIFEST_DIGEST_BYTES];
} w_seed_manifest_document;

/* Capacities travel with the immutable view so verify can reject truncation or
 * forged relations before following an index. Every record index is u32.
 * Fields are global owner-node/name order, edges are global owner-node/source
 * order, and canonical bytes are global scalar-node order without padding. */
typedef struct {
  const w_seed_manifest_document *documents;
  size_t document_count;
  size_t document_capacity;
  const w_seed_manifest_root *roots;
  size_t root_count;
  size_t root_capacity;
  const w_seed_manifest_node *nodes;
  size_t node_count;
  size_t node_capacity;
  const w_seed_manifest_field *fields;
  size_t field_count;
  size_t field_capacity;
  const w_seed_manifest_edge *edges;
  size_t edge_count;
  size_t edge_capacity;
  const uint8_t *canonical_bytes;
  size_t canonical_byte_count;
  size_t canonical_byte_capacity;
} w_seed_manifest_program;

typedef struct {
  w_seed_manifest_document *documents;
  size_t document_capacity;
  w_seed_manifest_root *roots;
  size_t root_capacity;
  w_seed_manifest_node *nodes;
  size_t node_capacity;
  w_seed_manifest_field *fields;
  size_t field_capacity;
  w_seed_manifest_edge *edges;
  size_t edge_capacity;
  uint8_t *canonical_bytes;
  size_t canonical_byte_capacity;
} w_seed_manifest_output;

typedef enum {
  /* Result-report baseline only. A callback cannot return NOT_CALLED. */
  W_SEED_MANIFEST_BACKEND_NOT_CALLED = 0,
  W_SEED_MANIFEST_BACKEND_OK,
  W_SEED_MANIFEST_BACKEND_CAPACITY,
  W_SEED_MANIFEST_BACKEND_LIMIT,
  W_SEED_MANIFEST_BACKEND_MUTATED,
  W_SEED_MANIFEST_BACKEND_BOUNDARY,
  W_SEED_MANIFEST_BACKEND_REPARSE,
  W_SEED_MANIFEST_BACKEND_UNSUPPORTED,
  W_SEED_MANIFEST_BACKEND_IO,
  W_SEED_MANIFEST_BACKEND_INVALID,
  W_SEED_MANIFEST_BACKEND_FAULT,
} w_seed_manifest_backend_status;

typedef enum {
  W_SEED_MANIFEST_BACKEND_PHASE_NONE = 0,
  W_SEED_MANIFEST_BACKEND_PHASE_VALIDATE,
  W_SEED_MANIFEST_BACKEND_PHASE_OPEN_CANDIDATE,
  W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_IDENTITY,
  W_SEED_MANIFEST_BACKEND_PHASE_READ,
  W_SEED_MANIFEST_BACKEND_PHASE_VERIFY_EOF,
  W_SEED_MANIFEST_BACKEND_PHASE_CLOSE,
} w_seed_manifest_backend_phase;

typedef struct {
  w_seed_manifest_backend_status status;
  w_seed_manifest_backend_phase phase;
  uint64_t generation;
  w_seed_owner_guard_candidate_ref candidate;
  size_t byte_count;
  size_t required_byte_capacity;
  uint8_t source_digest[W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t context_binding[W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t candidate_binding[W_SEED_MANIFEST_DIGEST_BYTES];
} w_seed_manifest_backend_result;

/* The API returns this report by value, so its terminal success or failure
 * report cannot alias caller storage. Every report has the exact schema.
 * Baseline backend is NOT_CALLED/NONE; owner_guard_status is significant only
 * when owner_guard_revalidate_called is true. document_index/candidate_index use
 * NONE and byte_offset uses NO_BYTE when not applicable. Counts, required byte
 * capacity, and digests are otherwise zero unless the operation rules publish
 * them. */
typedef struct {
  w_seed_manifest_status status;
  w_seed_manifest_phase phase;
  w_seed_manifest_error_kind error;
  w_seed_manifest_backend_status backend_status;
  w_seed_manifest_backend_phase backend_phase;
  w_seed_owner_guard_status owner_guard_status;
  uint32_t document_index;
  uint32_t candidate_index;
  size_t byte_offset;
  size_t required_byte_capacity;
  /* Set immediately before the sole OWN0 revalidate call, irrespective of its
   * outcome. A guarded retry is possible only while this is false. */
  bool owner_guard_revalidate_called;
  w_seed_manifest_counts required;
  w_seed_manifest_counts written;
  w_seed_manifest_limits limits;
  char schema[sizeof(W_SEED_MANIFEST_SCHEMA_VERSION)];
  uint8_t semantic_digest[W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t provenance_digest[W_SEED_MANIFEST_DIGEST_BYTES];
  uint8_t receipt_digest[W_SEED_MANIFEST_DIGEST_BYTES];
} w_seed_manifest_result;

/* Every result echoes generation and candidate. OK uses phase CLOSE, writes the
 * exact complete file, sets byte_count=required_byte_capacity, and reports the
 * MAN0 document source digest plus the derived bindings. CAPACITY uses phase
 * VERIFY_EOF, byte_count zero, exact required capacity greater than capacity,
 * and zero digests/bindings. LIMIT uses phase VERIFY_EOF, byte_count zero,
 * required=byte_limit+1, and zero digests/bindings. Other failures use the exact
 * failure phase and zero byte_count, required capacity, digests, and bindings.
 * A non-OK call may synchronously change a prefix within byte_capacity, except
 * the Windows UNSUPPORTED stub changes none. No callback writes outside
 * byte_capacity. It cannot retain or access context/bytes after return, start
 * asynchronous work, or mutate the OWN0 context. At byte_limit the adapter
 * performs one bounded EOF probe without storing the probe byte. More source
 * returns LIMIT, never CAPACITY or truncated OK. CAPACITY is retryable only
 * during the first wave. */
typedef w_seed_manifest_backend_result (*w_seed_manifest_read_candidate)(
    const void *context, uint64_t generation,
    w_seed_owner_guard_candidate_ref candidate, uint8_t *bytes,
    size_t byte_capacity, size_t byte_limit);

/* An operational descriptor binds to the original live guard. owner prevents
 * backend descriptor copies. guard, context, context_size, generation, and the
 * candidate set are validated before the first callback. A platform stub may
 * create only a fail-closed descriptor for direct callback tests; guarded_run
 * never treats that descriptor as operational. */
typedef struct w_seed_manifest_backend {
  const struct w_seed_manifest_backend *owner;
  const w_seed_owner_guard *guard;
  const void *context;
  size_t context_size;
  uint64_t generation;
  w_seed_manifest_read_candidate read_candidate;
} w_seed_manifest_backend;

typedef struct {
  uint8_t *first_bytes;
  size_t first_capacity;
  /* On guarded success, the written prefix of second_bytes becomes the exact
   * immutable backing of the published document.source view. */
  uint8_t *second_bytes;
  size_t second_capacity;
} w_seed_manifest_read_slot;

/* read_slot_capacity and staged_source_capacity must cover every guard
 * candidate. Each first buffer has nonzero capacity no greater than effective
 * max_document_bytes; a first-wave CAPACITY report permits a later call with
 * the exact required capacity. Before revalidation, each second buffer must
 * cover the matching measured first-wave byte count and must be no greater than
 * max_document_bytes. Embedded storage descriptors use declared containment;
 * every external backing is disjoint from all envelopes and other backings.
 * All ranges are checked for overflow and overlap before a callback. On
 * success, each second-wave written prefix is promoted to immutable source
 * backing; the caller must not mutate or reuse it while the program is live.
 * The first-wave buffer returns to scratch. Published records are copied once
 * after verification and stay bitwise unchanged on failure. */
typedef struct {
  w_seed_manifest_read_slot *read_slots;
  size_t read_slot_capacity;
  w_seed_manifest_source_input *staged_sources;
  size_t staged_source_capacity;
  w_seed_manifest_scratch scratch;
  w_seed_manifest_output staged;
  w_seed_manifest_output published;
} w_seed_manifest_guarded_storage;

typedef struct {
  w_seed_owner_guard *guard;
  const w_seed_manifest_backend *backend;
  w_seed_manifest_limits limits;
  w_seed_manifest_guarded_storage storage;
} w_seed_manifest_guarded_input;

/* Return the seed ceilings. A caller must copy and lower fields explicitly. */
w_seed_manifest_limits w_seed_manifest_default_limits(void);

/* Pure structural phases. They require canonical NONE bindings and do not use
 * a filesystem or owner policy. The returned result is always a complete
 * terminal report. Measure OK uses phase MEASURE, publishes required counts,
 * and leaves written/digests zero. Run OK uses phase RUN and publishes equal
 * required/written counts plus valid batch digests. On failure, counts and
 * output ranges are unchanged. Run reads but never writes its output descriptor;
 * only its backing ranges change at commit. A successful program borrows the
 * input source and output ranges; callers keep all those backings live and
 * immutable for the program lifetime. */
w_seed_manifest_result w_seed_manifest_measure(
    const w_seed_manifest_input *input, w_seed_manifest_counts *counts);

w_seed_manifest_result w_seed_manifest_run(
    const w_seed_manifest_input *input, w_seed_manifest_output *output);

/* Full verification accepts only an OK RUN or COMMIT report. It reparses source
 * and uses scratch to check exact uniqueness within the recorded work bound.
 * Program/result are read-only; scratch backings are mutable and must not alias
 * either descriptor or any backing. */
bool w_seed_manifest_verify(const w_seed_manifest_program *program,
                            const w_seed_manifest_result *result,
                            const w_seed_manifest_scratch *scratch);

/* Full verification for a committed guarded program. This exposes the same
 * guarded verifier used by guarded_run; it does not perform another owner
 * revalidation or change any MAN0 lifecycle. Program/result are read-only and
 * the explicit scratch backings may be reused. */
bool w_seed_manifest_guarded_verify(
    const w_seed_manifest_program *program,
    const w_seed_manifest_result *result,
    const w_seed_manifest_scratch *scratch);

/* This bridge accepts only an OK RUN or COMMIT report and validates only
 * descriptor, capacity, count, range, and alias envelopes. It never accepts a
 * MEASURE report and does not replace full verify. It constructs a local
 * candidate and writes program only at commit. Any failure preserves program
 * bitwise. */
bool w_seed_manifest_program_from_output(
    const w_seed_manifest_output *output,
    const w_seed_manifest_result *result,
    w_seed_manifest_program *program);

/* Requires the original LIVE_OBSERVED guard with candidates. An otherwise valid
 * input with a non-live guard returns status INVALID, phase VALIDATE, error NONE,
 * backend NOT_CALLED/NONE, and baseline owner-guard status. It leaves
 * owner_guard_revalidate_called false and calls no callback. The first wave is
 * fully measured and every scratch, second-wave, staged and published capacity
 * is preflighted before owner_guard_revalidate is called exactly once. The
 * second wave then reads the same retained identities, and length, bytes, the
 * backend digest and the independently recomputed core digest must all match.
 * After revalidation, LIMIT or a well-formed growth CAPACITY is terminal
 * MUTATED; a malformed CAPACITY is FAULT. Neither is a retry request. The
 * function never selects one candidate. The returned result always reports the
 * terminal outcome. Guarded OK uses phase COMMIT with equal required/written
 * counts and valid batch digests. Program and published ranges remain bitwise
 * unchanged on failure. */
w_seed_manifest_result w_seed_manifest_guarded_run(
    w_seed_manifest_guarded_input *input,
    w_seed_manifest_program *program);

#ifdef __cplusplus
}
#endif

#endif
