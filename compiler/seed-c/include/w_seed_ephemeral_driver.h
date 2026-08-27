#ifndef W_SEED_EPHEMERAL_DRIVER_H
#define W_SEED_EPHEMERAL_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_ephemeral_provider.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This is an internal, caller-owned orchestration API. It is not the public
 * `w check` command and it does not resolve packages or workspaces. */
#define W_SEED_EPHEMERAL_DRIVER_MAX_ROUNDS \
  W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES

typedef enum {
  W_SEED_EPHEMERAL_DRIVER_OK = 0,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY,
  W_SEED_EPHEMERAL_DRIVER_INVALID,
  W_SEED_EPHEMERAL_DRIVER_UNSUPPORTED,
  W_SEED_EPHEMERAL_DRIVER_IO,
} w_seed_ephemeral_driver_status;

typedef enum {
  W_SEED_EPHEMERAL_DRIVER_FAILURE_NONE = 0,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_POINTER,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_LIMIT,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_STORAGE,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_ROOT_SOURCE_ID,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_SOURCE_ID,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_MODULE_PATH,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_UNSUPPORTED_NFC,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_STD_PROVIDER,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_PROVIDER,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_MISSING_LOCAL,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSER_INIT,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_PARSE,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_SCAN,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_HEADER,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_GRAPH,
  W_SEED_EPHEMERAL_DRIVER_FAILURE_ORDER,
} w_seed_ephemeral_driver_failure;

typedef enum {
  W_SEED_EPHEMERAL_DRIVER_PHASE_NONE = 0,
  W_SEED_EPHEMERAL_DRIVER_PHASE_VALIDATE,
  W_SEED_EPHEMERAL_DRIVER_PHASE_ACQUIRE,
  W_SEED_EPHEMERAL_DRIVER_PHASE_PARSE,
  W_SEED_EPHEMERAL_DRIVER_PHASE_SCAN,
  W_SEED_EPHEMERAL_DRIVER_PHASE_DISCOVER,
  W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_MEASURE,
  W_SEED_EPHEMERAL_DRIVER_PHASE_GRAPH_WRITE,
  W_SEED_EPHEMERAL_DRIVER_PHASE_COMMIT,
} w_seed_ephemeral_driver_phase;

typedef enum {
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_NONE = 0,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SLOT,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_REQUEST,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_SOURCE_ID,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_MODULE_ID,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_NODE,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_TOKEN,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_FRAME,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PARSER_ISSUE,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_LEXER_FRAME,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_ORIGIN,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_CANDIDATE_DOCUMENT,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_CANDIDATE_FACT,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_INVENTORY,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_EDGE,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_ORDER,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_GRAPH_RESOLVED,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_DOCUMENT,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_ROUNDS,
  W_SEED_EPHEMERAL_DRIVER_CAPACITY_FIELD_PROVIDER,
} w_seed_ephemeral_driver_capacity_field;

/* Persistent per-source storage. The driver fills source, facts and document.
 * The two text buffers must be distinct caller-owned ranges. */
typedef struct {
  char *source_id_storage;
  size_t source_id_capacity;
  size_t source_id_length;
  char *module_id_storage;
  size_t module_id_capacity;
  size_t module_id_length;
  w_seed_cst_node *nodes;
  size_t node_capacity;
  w_seed_source source;
  w_seed_ephemeral_graph_provider_facts facts;
  w_seed_frontend_document document;
} w_seed_ephemeral_driver_slot;

/* All mutable driver state is caller-owned. Parser token/frame/issue scratch
 * and import-origin scratch are reused for one source at a time. CST nodes,
 * source bytes and provider token/byte buffers persist in each slot/request. */
typedef struct {
  w_seed_ephemeral_driver_slot *slots;
  size_t slot_capacity;
  w_seed_ephemeral_provider_request *requests;
  size_t request_capacity;
  w_seed_lexer_frame *lexer_frames;
  size_t lexer_frame_capacity;
  w_seed_parse_token *tokens;
  size_t token_capacity;
  w_seed_parse_frame *parse_frames;
  size_t parse_frame_capacity;
  w_seed_parse_issue *issues;
  size_t issue_capacity;
  w_seed_module_origin *origins;
  size_t origin_capacity;
  w_seed_frontend_document *candidate_documents;
  size_t candidate_document_capacity;
  w_seed_ephemeral_graph_provider_facts *candidate_facts;
  size_t candidate_fact_capacity;
  w_seed_ephemeral_graph_scratch *graph_scratch;
} w_seed_ephemeral_driver_scratch;

typedef struct {
  w_seed_byte_view root_path;
  /* This is logical identity. The driver never derives it from root_path. */
  w_seed_frontend_text root_source_id;
  w_seed_ephemeral_provider_limits provider_limits;
  size_t max_edges;
  size_t max_depth;
  w_seed_foreign_limits foreign_limits;
  w_seed_ephemeral_provider_backend backend;
} w_seed_ephemeral_driver_input;

typedef struct {
  w_seed_ephemeral_graph_output graph;
  /* Documents are copied in graph inventory order for a later frontend call. */
  w_seed_frontend_document *documents;
  size_t document_capacity;
  size_t document_count;
} w_seed_ephemeral_driver_output;

typedef struct {
  w_seed_ephemeral_driver_status status;
  w_seed_ephemeral_driver_failure failure;
  w_seed_ephemeral_driver_phase phase;
  size_t round;
  size_t candidate_index;
  size_t origin_index;
  size_t document_index;
  w_seed_span span;
  w_seed_ephemeral_driver_capacity_field capacity_field;
  size_t required_capacity;
  w_seed_ephemeral_provider_status provider_status;
  w_seed_ephemeral_provider_result provider_result;
  w_seed_parse_status parser_status;
  w_seed_parse_issue_kind parser_issue_kind;
  w_seed_module_scan_status scan_status;
  w_seed_module_scan_result scan_result;
  w_seed_ephemeral_graph_status graph_status;
  w_seed_ephemeral_graph_result graph_result;
} w_seed_ephemeral_driver_result;

/* Acquire and parse the transitive local source set, then publish the CHK4
 * graph and documents in logical order. The frontend is a separate stage. */
w_seed_ephemeral_driver_status w_seed_ephemeral_driver_run(
    const w_seed_ephemeral_driver_input *input,
    w_seed_ephemeral_driver_scratch *scratch,
    w_seed_ephemeral_driver_output *output,
    w_seed_ephemeral_driver_result *result);

#ifdef __cplusplus
}
#endif

#endif
