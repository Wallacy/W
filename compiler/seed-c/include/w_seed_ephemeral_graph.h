#ifndef W_SEED_EPHEMERAL_GRAPH_H
#define W_SEED_EPHEMERAL_GRAPH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_frontend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This is a caller-owned graph projection. It does not acquire sources or
 * retain any input storage. */
#define W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES 64u
#define W_SEED_EPHEMERAL_GRAPH_MAX_EDGES 4096u
#define W_SEED_EPHEMERAL_GRAPH_MAX_DEPTH 64u
#define W_SEED_EPHEMERAL_GRAPH_MAX_TOTAL_SOURCE_BYTES (16u * 1024u * 1024u)
#define W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES 32u

typedef enum {
  W_SEED_EPHEMERAL_GRAPH_OK = 0,
  W_SEED_EPHEMERAL_GRAPH_CAPACITY,
  W_SEED_EPHEMERAL_GRAPH_INVALID,
  W_SEED_EPHEMERAL_GRAPH_UNSUPPORTED,
} w_seed_ephemeral_graph_status;

/* Failure classes are stable enough for an adapter to select diagnostics.
 * The graph never embeds a physical path or provider display value. */
typedef enum {
  W_SEED_EPHEMERAL_GRAPH_FAILURE_NONE = 0,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_POINTER,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_SCRATCH,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_INDEX,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_SOURCE,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_IDENTITY,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_PATH,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_PROVIDER,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_SNAPSHOT,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_MISSING_LOCAL,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_STD_PROVIDER,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_CYCLE,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_LIMIT,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_ORDER,
  W_SEED_EPHEMERAL_GRAPH_FAILURE_UNSUPPORTED_NFC,
} w_seed_ephemeral_graph_failure;

typedef enum {
  W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE = 0,
  W_SEED_EPHEMERAL_GRAPH_SYMLINK_INSIDE,
  W_SEED_EPHEMERAL_GRAPH_SYMLINK_ESCAPE,
  W_SEED_EPHEMERAL_GRAPH_SYMLINK_UNPROVEN,
} w_seed_ephemeral_graph_symlink_state;

/* Tokens are opaque provider facts. Their storage remains caller-owned. */
typedef struct {
  w_seed_frontend_text provider_id;
  w_seed_frontend_text root_token;
  w_seed_frontend_text source_provider_owner_token;
  w_seed_frontend_text canonical_token;
  bool opened;
  bool containment_inside;
  w_seed_ephemeral_graph_symlink_state symlink;
  size_t snapshot_before_byte_count;
  uint8_t snapshot_before_digest[W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES];
  size_t snapshot_after_byte_count;
  uint8_t snapshot_after_digest[W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES];
} w_seed_ephemeral_graph_provider_facts;

/* These records are temporary graph-builder storage. The caller owns every
 * array and may place it in static, heap, or deliberately sized stack
 * storage. The builder stores only caller-owned text views in these records. */
typedef struct {
  size_t candidate_index;
  uint32_t depth;
  size_t source_bytes;
  w_seed_frontend_text source_id;
  w_seed_frontend_text module_id;
  w_seed_frontend_text local_module_name;
  uint8_t digest[W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES];
} w_seed_ephemeral_graph_scratch_node;

typedef struct {
  size_t source_node;
  size_t target_node;
  uint32_t direct_import_ordinal;
  w_seed_span declaration_span;
  w_seed_span path_span;
  w_seed_module_origin_kind logical_origin;
} w_seed_ephemeral_graph_scratch_edge;

/* Caller-owned scratch for one measure or write call. Capacities may be
 * smaller than the hard profile; a short array returns CAPACITY. A zero
 * capacity may use a NULL pointer when that projection is empty. Non-empty
 * arrays must be disjoint; overlapping scratch arrays are INVALID. */
typedef struct {
  w_seed_ephemeral_graph_scratch_node *nodes;
  size_t node_capacity;
  w_seed_ephemeral_graph_scratch_edge *edges;
  size_t edge_capacity;
  size_t *sorted_nodes;
  size_t sorted_nodes_capacity;
  size_t *node_ordinals;
  size_t node_ordinals_capacity;
  size_t *sorted_edges;
  size_t sorted_edges_capacity;
  size_t *sorted_resolved_edges;
  size_t sorted_resolved_edges_capacity;
  w_seed_module_origin *origins;
  size_t origin_capacity;
  uint32_t *indegree;
  size_t indegree_capacity;
  uint32_t *queue;
  size_t queue_capacity;
  uint32_t *depths;
  size_t depths_capacity;
} w_seed_ephemeral_graph_scratch;

typedef struct {
  const w_seed_frontend_document *documents;
  const w_seed_ephemeral_graph_provider_facts *provider_facts;
  size_t candidate_count;
  size_t root_candidate_index;
  w_seed_ephemeral_graph_scratch *scratch;
  /* max_sources must be non-zero. The other limits may be zero for an empty
   * edge/depth/byte projection. Every limit is explicit and within the hard
   * seed profile; a limit applies to the complete reachable graph. */
  size_t max_sources;
  size_t max_edges;
  size_t max_depth;
  size_t max_total_source_bytes;
} w_seed_ephemeral_graph_input;

typedef struct {
  w_seed_frontend_text source_id;
  w_seed_frontend_text module_id;
  w_seed_frontend_text local_module_name;
  uint8_t digest[W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES];
  uint32_t candidate_index;
  uint32_t depth;
} w_seed_ephemeral_graph_inventory_item;

typedef struct {
  uint32_t source_ordinal;
  uint32_t target_ordinal;
  uint32_t direct_import_ordinal;
  w_seed_span declaration_span;
  w_seed_span path_span;
  w_seed_module_origin_kind logical_origin;
} w_seed_ephemeral_graph_edge;

typedef struct {
  size_t sources;
  size_t edges;
  size_t total_source_bytes;
} w_seed_ephemeral_graph_counts;

typedef struct {
  w_seed_ephemeral_graph_inventory_item *inventory;
  size_t inventory_capacity;
  w_seed_ephemeral_graph_edge *edges;
  size_t edge_capacity;
  uint32_t *document_order;
  size_t document_order_capacity;
  w_seed_frontend_resolved_import *resolved_imports;
  size_t resolved_import_capacity;
} w_seed_ephemeral_graph_output;

typedef struct {
  w_seed_ephemeral_graph_status status;
  w_seed_ephemeral_graph_counts required;
  w_seed_ephemeral_graph_counts written;
  w_seed_ephemeral_graph_failure failure;
  size_t candidate_index;
  size_t document_ordinal;
  size_t edge_ordinal;
  w_seed_span span;
} w_seed_ephemeral_graph_result;

/* Compute exact requirements without writing any output. */
w_seed_ephemeral_graph_status w_seed_ephemeral_graph_measure(
    const w_seed_ephemeral_graph_input *input,
    w_seed_ephemeral_graph_counts *counts,
    w_seed_ephemeral_graph_result *result);

/* Compute and publish the complete projection only after all validation and
 * capacity checks pass. Every output buffer is unchanged on failure. */
w_seed_ephemeral_graph_status w_seed_ephemeral_graph_write(
    const w_seed_ephemeral_graph_input *input,
    w_seed_ephemeral_graph_output *output,
    w_seed_ephemeral_graph_result *result);

#ifdef __cplusplus
}
#endif

#endif
