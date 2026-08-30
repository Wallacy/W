#ifndef W_SEED_CHECK_STORAGE_H
#define W_SEED_CHECK_STORAGE_H

#include <stdbool.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_acquisition.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CHK9 preserves the ACQ0 capacities and adds only the JSON staging class. */
#define W_SEED_CHECK_STORAGE_MAX_SOURCES W_SEED_ACQUISITION_MAX_SOURCES
#define W_SEED_CHECK_STORAGE_MAX_SOURCE_BYTES \
  W_SEED_ACQUISITION_MAX_SOURCE_BYTES
#define W_SEED_CHECK_STORAGE_MAX_TOTAL_SOURCE_BYTES \
  W_SEED_ACQUISITION_MAX_TOTAL_SOURCE_BYTES
#define W_SEED_CHECK_STORAGE_MAX_NODES W_SEED_ACQUISITION_MAX_NODES
#define W_SEED_CHECK_STORAGE_MAX_TOTAL_NODES \
  W_SEED_ACQUISITION_MAX_TOTAL_NODES
#define W_SEED_CHECK_STORAGE_MAX_JSON_BYTES (64u * 1024u * 1024u)
#define W_SEED_CHECK_STORAGE_MAX_GROWTH_STEPS \
  W_SEED_ACQUISITION_MAX_GROWTH_STEPS
#define W_SEED_CHECK_STORAGE_MAX_RETRY_CLASSES                         \
  (W_SEED_ACQUISITION_MAX_RETRY_CLASSES == SIZE_MAX                   \
       ? SIZE_MAX                                                      \
       : W_SEED_ACQUISITION_MAX_RETRY_CLASSES + (size_t)1u)
#define W_SEED_CHECK_STORAGE_MAX_RETRIES                              \
  (W_SEED_CHECK_STORAGE_MAX_RETRY_CLASSES == SIZE_MAX                 \
       ? SIZE_MAX                                                      \
       : (W_SEED_CHECK_STORAGE_MAX_RETRY_CLASSES >                    \
                  (SIZE_MAX - (size_t)1u) /                           \
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

typedef w_seed_acquisition_allocate_fn w_seed_check_storage_allocate_fn;
typedef w_seed_acquisition_deallocate_fn w_seed_check_storage_deallocate_fn;

/* CHK9 owns one ACQ0 storage object and the separate D0 JSON pair. The child
 * allocator owns every allocation. This owning structure is non-copyable. */
typedef struct {
  w_seed_acquisition_storage acquisition;
  uint8_t *json_staging;
  uint8_t *json_final;
  size_t json_staging_capacity;
  size_t json_final_capacity;
} w_seed_check_storage;

/* Initialization requires a zero object and rejects live reinitialization
 * without modification. The allocator must be malloc-like: aligned returned
 * ranges are distinct from all other live allocations. */
bool w_seed_check_storage_init(w_seed_check_storage *storage);
bool w_seed_check_storage_init_with_allocator(
    w_seed_check_storage *storage,
    w_seed_check_storage_allocate_fn allocate,
    w_seed_check_storage_deallocate_fn deallocate);

w_seed_check_storage_status w_seed_check_storage_grow(
    w_seed_check_storage *storage, size_t request_index,
    size_t required_capacity);
w_seed_check_storage_status w_seed_check_storage_grow_nodes(
    w_seed_check_storage *storage, size_t request_index,
    size_t required_capacity);
w_seed_check_storage_status w_seed_check_storage_grow_json(
    w_seed_check_storage *storage, size_t required_capacity);

bool w_seed_check_storage_bind_request(
    const w_seed_check_storage *storage, size_t request_index,
    w_seed_ephemeral_provider_request *request);
bool w_seed_check_storage_bind_slot(const w_seed_check_storage *storage,
                                    size_t request_index,
                                    w_seed_ephemeral_driver_slot *slot);

/* Destroy releases JSON before ACQ0 and is idempotent. */
void w_seed_check_storage_destroy(w_seed_check_storage *storage);

#ifdef __cplusplus
}
#endif

#endif
