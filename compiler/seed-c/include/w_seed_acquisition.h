#ifndef W_SEED_ACQUISITION_H
#define W_SEED_ACQUISITION_H

#include <stdbool.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_ephemeral_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ACQ0 is an internal, caller-owned acquisition layer. It does not select a
 * filesystem policy, resolve a project, call the frontend, or publish a CLI. */
#define W_SEED_ACQUISITION_MAX_SOURCES \
  W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES
#define W_SEED_ACQUISITION_MAX_SOURCE_BYTES \
  W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES
#define W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES \
  W_SEED_EPHEMERAL_PROVIDER_MAX_TOTAL_SOURCE_BYTES
#define W_SEED_ACQUISITION_MAX_NODES W_SEED_FRONTEND_MAX_CST_NODES
#define W_SEED_ACQUISITION_MAX_TOTAL_NODES 262144u

/* One initial driver call plus one bounded growth step for each resizable
 * provider-byte and parser-node class. */
#define W_SEED_ACQUISITION_MAX_GROWTH_STEPS \
  ((sizeof(size_t) * (size_t)CHAR_BIT) + (size_t)1u)
#define W_SEED_ACQUISITION_MAX_RETRY_CLASSES                         \
  ((size_t)W_SEED_ACQUISITION_MAX_SOURCES >                          \
           (SIZE_MAX - (size_t)1u) / (size_t)2u                      \
       ? SIZE_MAX                                                     \
       : ((size_t)2u * (size_t)W_SEED_ACQUISITION_MAX_SOURCES))
#define W_SEED_ACQUISITION_MAX_ATTEMPTS                              \
  (W_SEED_ACQUISITION_MAX_RETRY_CLASSES == SIZE_MAX                  \
       ? SIZE_MAX                                                     \
       : (W_SEED_ACQUISITION_MAX_RETRY_CLASSES >                     \
                  (SIZE_MAX - (size_t)1u) /                          \
                      W_SEED_ACQUISITION_MAX_GROWTH_STEPS            \
              ? SIZE_MAX                                              \
              : ((size_t)1u +                                        \
                 W_SEED_ACQUISITION_MAX_RETRY_CLASSES *              \
                     W_SEED_ACQUISITION_MAX_GROWTH_STEPS)))

_Static_assert(W_SEED_ACQUISITION_MAX_RETRY_CLASSES != SIZE_MAX,
               "acquisition retry class count overflows size_t");
_Static_assert(W_SEED_ACQUISITION_MAX_ATTEMPTS != SIZE_MAX,
               "acquisition attempt budget overflows size_t");

typedef enum {
  W_SEED_ACQUISITION_STORAGE_OK = 0,
  W_SEED_ACQUISITION_STORAGE_INVALID,
  W_SEED_ACQUISITION_STORAGE_CAPACITY,
  W_SEED_ACQUISITION_STORAGE_ALLOCATION,
} w_seed_acquisition_storage_status;

typedef void *(*w_seed_acquisition_allocate_fn)(size_t size);
typedef void (*w_seed_acquisition_deallocate_fn)(void *pointer);

/* Published source and CST views depend on these arenas and on the root-path,
 * root-SourceId, source-ID, module-ID, token, slot, request, parser scratch,
 * graph scratch, graph output, document output, and backend-context backings
 * supplied to the pipeline. A successful grow, another pipeline call, backing
 * mutation, reuse, or destroy invalidates all views from an older call. A
 * failed or no-op grow preserves existing views.
 *
 * This owning structure is non-copyable. Its allocator must be malloc-like:
 * every nonzero allocation is suitably aligned and distinct from all other
 * live allocations. Storage and all pipeline backings require exclusive
 * access for the complete call. Grow, destroy, concurrent use, reentrant use,
 * and backing mutation are forbidden while a pipeline driver call is active. */
typedef struct w_seed_acquisition_storage {
  uint8_t *staging_bytes[W_SEED_ACQUISITION_MAX_SOURCES];
  uint8_t *revalidation_bytes[W_SEED_ACQUISITION_MAX_SOURCES];
  uint8_t *published_bytes[W_SEED_ACQUISITION_MAX_SOURCES];
  w_seed_cst_node *nodes[W_SEED_ACQUISITION_MAX_SOURCES];
  size_t staging_capacity[W_SEED_ACQUISITION_MAX_SOURCES];
  size_t revalidation_capacity[W_SEED_ACQUISITION_MAX_SOURCES];
  size_t published_capacity[W_SEED_ACQUISITION_MAX_SOURCES];
  size_t node_capacity[W_SEED_ACQUISITION_MAX_SOURCES];
  size_t staging_total;
  size_t revalidation_total;
  size_t published_total;
  size_t node_total;
  w_seed_acquisition_allocate_fn allocate;
  w_seed_acquisition_deallocate_fn deallocate;
  const struct w_seed_acquisition_storage *owner;
  bool initialized;
  bool pipeline_active;
} w_seed_acquisition_storage;

/* Initialization requires a zero object. A live or otherwise nonzero object
 * is rejected without modification. */
bool w_seed_acquisition_storage_init(w_seed_acquisition_storage *storage);
bool w_seed_acquisition_storage_init_with_allocator(
    w_seed_acquisition_storage *storage,
    w_seed_acquisition_allocate_fn allocate,
    w_seed_acquisition_deallocate_fn deallocate);

/* Growth is monotonic, bounded, and transactional. Existing bytes remain
 * unchanged when allocation or capacity checks fail. */
w_seed_acquisition_storage_status w_seed_acquisition_storage_grow_source(
    w_seed_acquisition_storage *storage, size_t request_index,
    size_t required_capacity);
w_seed_acquisition_storage_status w_seed_acquisition_storage_grow_nodes(
    w_seed_acquisition_storage *storage, size_t request_index,
    size_t required_capacity);

bool w_seed_acquisition_storage_bind_request(
    const w_seed_acquisition_storage *storage, size_t request_index,
    w_seed_ephemeral_provider_request *request);
bool w_seed_acquisition_storage_bind_slot(
    const w_seed_acquisition_storage *storage, size_t request_index,
    w_seed_ephemeral_driver_slot *slot);

/* Validate all slots and requests before changing either array. Then bind all
 * resizable ACQ0 arenas for one complete driver attempt. */
bool w_seed_acquisition_storage_bind_driver(
    const w_seed_acquisition_storage *storage,
    w_seed_ephemeral_driver_scratch *scratch);

void w_seed_acquisition_storage_destroy(w_seed_acquisition_storage *storage);

typedef enum {
  W_SEED_ACQUISITION_RETRY_NOT_RUN = 0,
  W_SEED_ACQUISITION_RETRY_OK,
  W_SEED_ACQUISITION_RETRY_CAPACITY,
  W_SEED_ACQUISITION_RETRY_ALLOCATION,
  W_SEED_ACQUISITION_RETRY_INVALID,
  W_SEED_ACQUISITION_RETRY_FAULT,
} w_seed_acquisition_retry_status;

typedef enum {
  W_SEED_ACQUISITION_RETRY_ACTION_NOT_RUN = 0,
  W_SEED_ACQUISITION_RETRY_RETRY,
  W_SEED_ACQUISITION_RETRY_TERMINAL,
} w_seed_acquisition_retry_action;

typedef enum {
  W_SEED_ACQUISITION_RETRY_DETAIL_NONE = 0,
  W_SEED_ACQUISITION_RETRY_DETAIL_NON_RESIZABLE,
  W_SEED_ACQUISITION_RETRY_DETAIL_INVALID_RESULT,
  W_SEED_ACQUISITION_RETRY_DETAIL_NO_PROGRESS,
  W_SEED_ACQUISITION_RETRY_DETAIL_CAPACITY,
  W_SEED_ACQUISITION_RETRY_DETAIL_ALLOCATION,
  W_SEED_ACQUISITION_RETRY_DETAIL_STORAGE,
  W_SEED_ACQUISITION_RETRY_DETAIL_RETRY_LIMIT,
} w_seed_acquisition_retry_detail;

typedef struct {
  w_seed_acquisition_retry_status status;
  w_seed_acquisition_retry_action action;
  w_seed_acquisition_retry_detail detail;
  const char *reason;
} w_seed_acquisition_retry_outcome;

/* Apply one retry action to a complete CHK6 capacity envelope. */
w_seed_acquisition_retry_outcome w_seed_acquisition_retry_apply(
    w_seed_acquisition_storage *storage,
    w_seed_ephemeral_driver_status driver_status,
    const w_seed_ephemeral_driver_result *driver_result);

typedef struct {
  const w_seed_ephemeral_driver_input *driver_input;
  w_seed_ephemeral_driver_scratch *scratch;
  w_seed_ephemeral_driver_output *output;
  w_seed_acquisition_storage *storage;
  /* driver_input->backend.context and this size form one canonical pair.
   * Callbacks may write only their explicit out-parameters and this complete
   * declared byte range. It must not contain hidden mutable pointees or
   * overlap any other pipeline range. Read-only hidden pointees are allowed. */
  size_t backend_context_size;
} w_seed_acquisition_pipeline_input;

typedef enum {
  W_SEED_ACQUISITION_PIPELINE_OK = 0,
  W_SEED_ACQUISITION_PIPELINE_CAPACITY,
  W_SEED_ACQUISITION_PIPELINE_UNSUPPORTED,
  W_SEED_ACQUISITION_PIPELINE_INVALID,
  W_SEED_ACQUISITION_PIPELINE_IO,
  W_SEED_ACQUISITION_PIPELINE_ALLOCATION,
  W_SEED_ACQUISITION_PIPELINE_FAULT,
} w_seed_acquisition_pipeline_status;

typedef struct {
  w_seed_acquisition_pipeline_status status;
  size_t attempts;
  w_seed_ephemeral_driver_result driver_result;
  /* NOT_RUN means no capacity result required a retry decision. Otherwise,
   * this is the last decision applied by the bounded loop. */
  w_seed_acquisition_retry_outcome retry;
  size_t document_count;
  w_seed_ephemeral_graph_counts graph_written;
} w_seed_acquisition_pipeline_result;

/* Run bounded bind -> CHK6 driver -> retry. The driver output descriptor and
 * its published arrays remain bitwise unchanged on every non-OK return.
 * Scratch, storage, and bound descriptors may change. On OK, document_count
 * and graph_written delimit the written output ranges. The latest stable wave
 * is not a global filesystem snapshot across all CHK6 waves. */
w_seed_acquisition_pipeline_status w_seed_acquisition_pipeline_run(
    const w_seed_acquisition_pipeline_input *input,
    w_seed_acquisition_pipeline_result *result);

#ifdef __cplusplus
}
#endif

#endif
