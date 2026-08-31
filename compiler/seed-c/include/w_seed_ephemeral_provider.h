#ifndef W_SEED_EPHEMERAL_PROVIDER_H
#define W_SEED_EPHEMERAL_PROVIDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_ephemeral_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CHK5 provider limits. The caller may select lower limits per invocation. */
#define W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES \
  W_SEED_EPHEMERAL_GRAPH_MAX_SOURCES
#define W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES \
  W_SEED_EPHEMERAL_GRAPH_MAX_TOTAL_SOURCE_BYTES
#define W_SEED_EPHEMERAL_PROVIDER_MAX_TOTAL_SOURCE_BYTES \
  W_SEED_EPHEMERAL_GRAPH_MAX_TOTAL_SOURCE_BYTES
#define W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES 4096u
#define W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES 4096u

typedef enum {
  W_SEED_EPHEMERAL_PROVIDER_OK = 0,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY,
  W_SEED_EPHEMERAL_PROVIDER_INVALID,
  W_SEED_EPHEMERAL_PROVIDER_UNSUPPORTED,
  W_SEED_EPHEMERAL_PROVIDER_IO,
} w_seed_ephemeral_provider_status;

typedef enum {
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_NONE = 0,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_POINTER,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_PATH,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_INVALID_UTF8,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_UNSUPPORTED_NFC,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_ROOT,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_SOURCE_MISSING,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_SOURCE_OPEN,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_SOURCE_READ,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_SOURCE_ENCODING,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_TOKEN,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_CONTAINMENT,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_CANONICAL_ALIAS,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_SNAPSHOT,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND,
  W_SEED_EPHEMERAL_PROVIDER_FAILURE_ORDER,
} w_seed_ephemeral_provider_failure;

typedef struct {
  size_t max_sources;
  size_t max_source_bytes;
  size_t max_total_source_bytes;
  size_t max_path_bytes;
  size_t max_token_bytes;
} w_seed_ephemeral_provider_limits;

/* The backend writes token text into these caller-owned scratch buffers. A
 * token is non-empty, bounded, valid UTF-8, and contains no NUL byte. The
 * current CHK4 projection uses ASCII token values. */
typedef struct {
  char *provider_id;
  size_t provider_id_capacity;
  char *root_token;
  size_t root_token_capacity;
  char *source_provider_owner_token;
  size_t source_provider_owner_token_capacity;
  char *canonical_token;
  size_t canonical_token_capacity;
} w_seed_ephemeral_provider_token_buffers;

typedef struct {
  size_t required_capacity;
  size_t maximum_emitted_length;
} w_seed_ephemeral_provider_token_capacity;

/* Every successful callback must fit each token in the declared maximum.
 * The provider preflights both token sets against required_capacity before any
 * handle is opened. required_capacity is at least maximum_emitted_length.
 * maximum_emitted_length is backend metadata, not a result field. A token
 * callback must not return BACKEND_CAPACITY after that preflight; it must
 * return BACKEND_INVALID if its own contract fails. */
typedef struct {
  w_seed_ephemeral_provider_token_capacity provider_id;
  w_seed_ephemeral_provider_token_capacity root_token;
  w_seed_ephemeral_provider_token_capacity source_provider_owner_token;
  w_seed_ephemeral_provider_token_capacity canonical_token;
} w_seed_ephemeral_provider_metadata;

/* A backend handle is an opaque integer representation. uintptr_t is the
 * portable representation used by both the C23 primary lane and the explicit
 * C11 recovery lane; it can carry a backend object pointer or a native handle
 * without a narrowing conversion. The provider never dereferences it. */
typedef struct {
  uintptr_t value;
} w_seed_ephemeral_provider_handle;

/* One observation is returned by open and by revalidation. Token bytes are in
 * the token buffers passed to that callback. */
typedef struct {
  bool opened;
  bool containment_inside;
  w_seed_ephemeral_graph_symlink_state symlink;
  size_t provider_id_length;
  size_t root_token_length;
  size_t source_provider_owner_token_length;
  size_t canonical_token_length;
} w_seed_ephemeral_provider_observation;

typedef enum {
  W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK = 0,
  W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND,
  W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY,
  W_SEED_EPHEMERAL_PROVIDER_BACKEND_ESCAPE,
  W_SEED_EPHEMERAL_PROVIDER_BACKEND_SYMLINK,
  W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED,
  W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO,
  W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID,
} w_seed_ephemeral_provider_backend_status;

/* The vtable is synchronous, stateless from the provider's perspective, and
 * must not discover or retain a source outside the requested SourceId. The
 * root path is physical input. A source_id is a logical root-relative ID.
 * On BACKEND_OK, open_root must return two distinct non-zero handles and
 * open_source must return a non-zero handle not already owned by this plan. A
 * callback that returns a status other than BACKEND_OK transfers no handle.
 * BACKEND_CAPACITY is reserved for read_source/revalidate_source when their
 * byte output is larger than the supplied byte capacity; open callbacks have
 * no capacity escape after token preflight.
 * close_source and close_root do not fail and the provider calls each exactly
 * once for every handle returned by a successful open callback. */
typedef struct {
  void *context;
  w_seed_ephemeral_provider_backend_status (*open_root)(
      void *context, w_seed_byte_view root_path,
      w_seed_ephemeral_provider_token_buffers *tokens,
      w_seed_ephemeral_provider_handle *root_handle,
      w_seed_ephemeral_provider_handle *root_source_handle,
      w_seed_ephemeral_provider_observation *observation);
  w_seed_ephemeral_provider_backend_status (*open_source)(
      void *context, w_seed_ephemeral_provider_handle root_handle,
      w_seed_frontend_text source_id,
      w_seed_ephemeral_provider_token_buffers *tokens,
      w_seed_ephemeral_provider_handle *source_handle,
      w_seed_ephemeral_provider_observation *observation);
  w_seed_ephemeral_provider_backend_status (*read_source)(
      void *context, w_seed_ephemeral_provider_handle source_handle,
      uint8_t *bytes, size_t capacity, size_t *written);
  w_seed_ephemeral_provider_backend_status (*revalidate_source)(
      void *context, w_seed_ephemeral_provider_handle root_handle,
      w_seed_ephemeral_provider_handle source_handle,
      w_seed_frontend_text source_id,
      w_seed_ephemeral_provider_token_buffers *tokens,
      w_seed_ephemeral_provider_observation *observation, uint8_t *bytes,
      size_t capacity, size_t *written);
  void (*close_source)(void *context,
                       w_seed_ephemeral_provider_handle source_handle);
  void (*close_root)(void *context, w_seed_ephemeral_provider_handle root_handle);
  w_seed_ephemeral_provider_metadata metadata;
} w_seed_ephemeral_provider_backend;

/* The provider writes only after every request passes acquisition and
 * revalidation. staging_bytes and revalidation_bytes are scratch. The primary
 * tokens buffer is also the backing storage for every text view in facts after
 * success; it must outlive those facts and remain valid while they are used.
 * revalidation_tokens is scratch only. Both token buffers and both byte
 * scratch buffers may change on failure. bytes, source, and facts are
 * published outputs and remain bitwise unchanged on failure. */
typedef struct {
  w_seed_frontend_text source_id;
  uint8_t *staging_bytes;
  size_t staging_capacity;
  uint8_t *revalidation_bytes;
  size_t revalidation_capacity;
  uint8_t *bytes;
  size_t byte_capacity;
  w_seed_source *source;
  w_seed_ephemeral_graph_provider_facts *facts;
  w_seed_ephemeral_provider_token_buffers tokens;
  w_seed_ephemeral_provider_token_buffers revalidation_tokens;
} w_seed_ephemeral_provider_request;

typedef struct {
  w_seed_byte_view root_path;
  const w_seed_ephemeral_provider_request *requests;
  size_t request_count;
  size_t root_request_index;
  w_seed_ephemeral_provider_limits limits;
  w_seed_ephemeral_provider_backend backend;
} w_seed_ephemeral_provider_input;

typedef enum {
  W_SEED_EPHEMERAL_PROVIDER_PHASE_NONE = 0,
  W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE,
  W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT,
  W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE,
  W_SEED_EPHEMERAL_PROVIDER_PHASE_READ,
  W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE,
  W_SEED_EPHEMERAL_PROVIDER_PHASE_COMMIT,
  W_SEED_EPHEMERAL_PROVIDER_PHASE_CLOSE,
} w_seed_ephemeral_provider_phase;

typedef enum {
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_NONE = 0,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_AGGREGATE_SOURCE_BYTES,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_PROVIDER_ID,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_ROOT_TOKEN,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_SOURCE_PROVIDER_OWNER_TOKEN,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_CANONICAL_TOKEN,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_PROVIDER_ID,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_ROOT_TOKEN,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_SOURCE_PROVIDER_OWNER_TOKEN,
  W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_CANONICAL_TOKEN,
} w_seed_ephemeral_provider_capacity_field;

typedef struct {
  w_seed_ephemeral_provider_status status;
  w_seed_ephemeral_provider_failure failure;
  w_seed_ephemeral_provider_phase phase;
  size_t request_index;
  size_t total_source_bytes;
  w_seed_ephemeral_provider_capacity_field capacity_field;
  size_t required_capacity;
  /* Kept as a byte-only compatibility alias. required_capacity is canonical. */
  size_t required_byte_capacity;
  size_t observed_byte_count;
  w_seed_ephemeral_provider_backend_status backend_status;
} w_seed_ephemeral_provider_result;

/* Acquire only the explicit root and SourceIds, then revalidate every source
 * before publishing bytes, source views, and CHK4 provider facts. `result`
 * must be a distinct caller-owned object, disjoint from input and request
 * storage; an overlap is rejected before result is written. */
w_seed_ephemeral_provider_status w_seed_ephemeral_provider_acquire(
    const w_seed_ephemeral_provider_input *input,
    w_seed_ephemeral_provider_result *result);

#ifdef __cplusplus
}
#endif

#endif
