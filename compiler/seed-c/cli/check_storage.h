#ifndef W_SEED_CHECK_STORAGE_H
#define W_SEED_CHECK_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include "w_seed_ephemeral_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The public check edge keeps the provider limits unchanged.  This storage
 * owns only resizable provider/D0 arenas.  All graph, parser, and frontend
 * arrays remain caller-owned by their respective CHK6/CHK7 inputs. */
#define W_SEED_CHECK_STORAGE_MAX_SOURCES \
  W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES
#define W_SEED_CHECK_STORAGE_MAX_SOURCE_BYTES \
  W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES
#define W_SEED_CHECK_STORAGE_MAX_TOTAL_SOURCE_BYTES \
  W_SEED_EPHEMERAL_PROVIDER_MAX_TOTAL_SOURCE_BYTES
/* A document is accepted by the CHK6/CHK7 frontend only up to its CST
 * profile.  The legacy checker arena remains the aggregate budget across
 * documents, so one source keeps the same effective successful limit while
 * multi-source runs share the larger bounded pool. */
#define W_SEED_CHECK_STORAGE_MAX_NODES W_SEED_FRONTEND_MAX_CST_NODES
#define W_SEED_CHECK_STORAGE_MAX_TOTAL_NODES 262144u
/* D0 JSON is kept outside the static frontend profile.  Both buffers use
 * this bootstrap ceiling and are grown as a pair so a retry can never expose
 * half of a JSON transaction. */
#define W_SEED_CHECK_STORAGE_MAX_JSON_BYTES (64u * 1024u * 1024u)
/* One initial composition attempt plus one bounded geometric growth step for
 * each source-byte class, source-node class, and the JSON class.  The
 * conditional expressions saturate before multiplication; the assertions
 * below make an unsupported size_t/configuration fail at compile time rather
 * than wrap the retry budget. */
#define W_SEED_CHECK_STORAGE_MAX_GROWTH_STEPS \
  ((sizeof(size_t) * (size_t)CHAR_BIT) + (size_t)1u)
#define W_SEED_CHECK_STORAGE_MAX_RETRY_CLASSES                         \
  ((size_t)W_SEED_CHECK_STORAGE_MAX_SOURCES >                         \
           (SIZE_MAX - (size_t)1u) / (size_t)2u                       \
       ? SIZE_MAX                                                       \
       : ((size_t)2u * (size_t)W_SEED_CHECK_STORAGE_MAX_SOURCES +      \
          (size_t)1u))
#define W_SEED_CHECK_STORAGE_MAX_RETRIES \
  (W_SEED_CHECK_STORAGE_MAX_RETRY_CLASSES == SIZE_MAX                \
       ? SIZE_MAX                                                     \
       : (W_SEED_CHECK_STORAGE_MAX_RETRY_CLASSES >                    \
                  (SIZE_MAX - (size_t)1u) /                          \
                      W_SEED_CHECK_STORAGE_MAX_GROWTH_STEPS           \
              ? SIZE_MAX                                               \
              : ((size_t)1u + W_SEED_CHECK_STORAGE_MAX_RETRY_CLASSES * \
                                      W_SEED_CHECK_STORAGE_MAX_GROWTH_STEPS)))

_Static_assert(W_SEED_CHECK_STORAGE_MAX_RETRY_CLASSES != SIZE_MAX,
               "check retry class count overflows size_t");
_Static_assert(W_SEED_CHECK_STORAGE_MAX_RETRIES != SIZE_MAX,
               "check retry budget overflows size_t");

typedef enum {
  W_SEED_CHECK_STORAGE_OK = 0,
  W_SEED_CHECK_STORAGE_INVALID,
  W_SEED_CHECK_STORAGE_CAPACITY,
  W_SEED_CHECK_STORAGE_ALLOCATION,
} w_seed_check_storage_status;

/* This seam is private to the CLI storage module.  The production
 * initializer installs malloc/free; the test initializer can force a
 * deterministic failure after any allocation. */
typedef void *(*w_seed_check_storage_allocate_fn)(size_t size);
typedef void (*w_seed_check_storage_deallocate_fn)(void *pointer);

/* A zero capacity is a valid initial provider request.  The provider returns
 * the required byte capacity without publishing a source, and the CLI can
 * then grow this object and retry the complete composition.  The three
 * arenas have separate aggregate limits, even though one request grows all
 * three to the same capacity. */
typedef struct {
  uint8_t *staging_bytes[W_SEED_CHECK_STORAGE_MAX_SOURCES];
  uint8_t *revalidation_bytes[W_SEED_CHECK_STORAGE_MAX_SOURCES];
  uint8_t *published_bytes[W_SEED_CHECK_STORAGE_MAX_SOURCES];
  w_seed_cst_node *nodes[W_SEED_CHECK_STORAGE_MAX_SOURCES];
  size_t staging_capacity[W_SEED_CHECK_STORAGE_MAX_SOURCES];
  size_t revalidation_capacity[W_SEED_CHECK_STORAGE_MAX_SOURCES];
  size_t published_capacity[W_SEED_CHECK_STORAGE_MAX_SOURCES];
  size_t node_capacity[W_SEED_CHECK_STORAGE_MAX_SOURCES];
  size_t staging_total;
  size_t revalidation_total;
  size_t published_total;
  size_t node_total;
  uint8_t *json_staging;
  uint8_t *json_final;
  size_t json_staging_capacity;
  size_t json_final_capacity;
  w_seed_check_storage_allocate_fn allocate;
  w_seed_check_storage_deallocate_fn deallocate;
  bool initialized;
} w_seed_check_storage;

/* Initialize caller-owned storage.  The caller must destroy a live object
 * before initializing that object again. */
bool w_seed_check_storage_init(w_seed_check_storage *storage);

/* Initialize with an internal test or embedding allocator.  This is not part
 * of the public `w` command surface; it exists to prove allocation rollback
 * without depending on process memory pressure. */
bool w_seed_check_storage_init_with_allocator(
    w_seed_check_storage *storage,
    w_seed_check_storage_allocate_fn allocate,
    w_seed_check_storage_deallocate_fn deallocate);

/* Grow all three byte buffers for one provider request.  The target is
 * monotonic and normally rounds geometrically, with an exact-capacity fallback
 * at the aggregate limit so valid source sets are not rejected because of
 * rounding.  Existing contents remain unchanged.  The operation is
 * all-or-nothing: an allocation failure leaves the object and every old
 * buffer unchanged.  If the requested capacity is already covered, it
 * returns OK without changing storage; a retry loop must treat that no-op as
 * an internal invariant failure instead of retrying forever. */
w_seed_check_storage_status w_seed_check_storage_grow(
    w_seed_check_storage *storage, size_t request_index,
    size_t required_capacity);

/* Grow the caller-owned CST node arena for one source.  Node capacity is
 * bounded per source by W_SEED_CHECK_STORAGE_MAX_NODES and in aggregate by
 * W_SEED_CHECK_STORAGE_MAX_TOTAL_NODES. */
w_seed_check_storage_status w_seed_check_storage_grow_nodes(
    w_seed_check_storage *storage, size_t request_index,
    size_t required_capacity);

/* Grow the two D0 buffers to the same capacity.  Zero capacity is valid and
 * a covered requirement is a no-op that the retry coordinator must reject. */
w_seed_check_storage_status w_seed_check_storage_grow_json(
    w_seed_check_storage *storage, size_t required_capacity);

/* Copy the current byte arenas for one request into a provider request.  The
 * source, facts, and token fields in request are intentionally untouched. */
bool w_seed_check_storage_bind_request(
    const w_seed_check_storage *storage, size_t request_index,
    w_seed_ephemeral_provider_request *request);

/* Bind the CST arena for one CHK6 driver slot.  Other slot fields remain
 * untouched so the driver can continue to own source identity and facts. */
bool w_seed_check_storage_bind_slot(const w_seed_check_storage *storage,
                                    size_t request_index,
                                    w_seed_ephemeral_driver_slot *slot);

/* Release every buffer, including buffers from a partially completed grow. */
void w_seed_check_storage_destroy(w_seed_check_storage *storage);

#ifdef __cplusplus
}
#endif

#endif
