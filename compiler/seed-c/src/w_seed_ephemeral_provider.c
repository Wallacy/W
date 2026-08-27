#include "w_seed_ephemeral_provider.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8,
               "w_seed_ephemeral_provider requires 8-bit bytes");

#ifndef UINTPTR_MAX
#error "w_seed_ephemeral_provider requires a C11 uintptr_t representation"
#endif

typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool present;
} provider_range;

typedef struct {
  w_seed_source sources[W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES];
  w_seed_ephemeral_graph_provider_facts facts
      [W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES];
  w_seed_ephemeral_provider_observation observations
      [W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES];
  w_seed_ephemeral_provider_handle source_handles
      [W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES];
  size_t byte_counts[W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES];
  uint8_t before_digest[W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES][32];
  uint8_t after_digest[W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES][32];
  bool source_open[W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES];
  bool root_open;
  w_seed_ephemeral_provider_handle root_handle;
  size_t total_source_bytes;
} provider_plan;

_Static_assert(sizeof(provider_plan) < (size_t)(2u * 1024u * 1024u),
               "ephemeral provider plan must remain bounded");

typedef enum {
  PATH_CHECK_OK = 0,
  PATH_CHECK_INVALID,
  PATH_CHECK_UTF8,
  PATH_CHECK_UNSUPPORTED_NFC,
} path_check;

static void clear_result(w_seed_ephemeral_provider_result *result) {
  if (result == NULL) return;
  (void)memset(result, 0, sizeof(*result));
  result->status = W_SEED_EPHEMERAL_PROVIDER_INVALID;
  result->failure = W_SEED_EPHEMERAL_PROVIDER_FAILURE_NONE;
  result->phase = W_SEED_EPHEMERAL_PROVIDER_PHASE_NONE;
  result->request_index = SIZE_MAX;
  result->capacity_field =
      W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_NONE;
  result->required_capacity = 0u;
  result->backend_status = W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_status fail_result(
    w_seed_ephemeral_provider_result *result,
    w_seed_ephemeral_provider_status status,
    w_seed_ephemeral_provider_failure failure,
    w_seed_ephemeral_provider_phase phase, size_t request_index,
    size_t required_byte_capacity, size_t observed_byte_count,
    w_seed_ephemeral_provider_backend_status backend_status) {
  if (result != NULL) {
    result->status = status;
    result->failure = failure;
    result->phase = phase;
    result->request_index = request_index;
    result->required_byte_capacity = required_byte_capacity;
    result->observed_byte_count = observed_byte_count;
    result->backend_status = backend_status;
  }
  return status;
}

static bool capacity_field_is_bytes(
    w_seed_ephemeral_provider_capacity_field field) {
  return field == W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES ||
         field == W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES ||
         field == W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES;
}

static w_seed_ephemeral_provider_status fail_capacity_result(
    w_seed_ephemeral_provider_result *result,
    w_seed_ephemeral_provider_status status,
    w_seed_ephemeral_provider_failure failure,
    w_seed_ephemeral_provider_phase phase, size_t request_index,
    w_seed_ephemeral_provider_capacity_field field, size_t required_capacity,
    size_t observed_byte_count,
    w_seed_ephemeral_provider_backend_status backend_status) {
  const w_seed_ephemeral_provider_status failed =
      fail_result(result, status, failure, phase, request_index,
                  capacity_field_is_bytes(field) ? required_capacity : 0u,
                  observed_byte_count, backend_status);
  if (result != NULL) {
    result->capacity_field = field;
    result->required_capacity = required_capacity;
  }
  return failed;
}

static bool text_equal(w_seed_frontend_text left,
                       w_seed_frontend_text right) {
  return left.length == right.length &&
         (left.length == 0u ||
          memcmp(left.data, right.data, left.length) == 0);
}

static bool contains_nul(const uint8_t *data, size_t length) {
  if (length != 0u && data == NULL) return true;
  for (size_t index = 0u; index < length; index += 1u) {
    if (data[index] == 0u) return true;
  }
  return false;
}

static bool ascii_identifier_byte(uint8_t byte) {
  return (byte >= (uint8_t)'a' && byte <= (uint8_t)'z') ||
         (byte >= (uint8_t)'A' && byte <= (uint8_t)'Z') || byte == (uint8_t)'_';
}

static bool ascii_identifier_tail_byte(uint8_t byte) {
  return ascii_identifier_byte(byte) ||
         (byte >= (uint8_t)'0' && byte <= (uint8_t)'9');
}

static bool ascii_identifier_range(const char *data, size_t start,
                                   size_t end) {
  if (data == NULL || start >= end ||
      !ascii_identifier_byte((uint8_t)data[start]))
    return false;
  for (size_t index = start + 1u; index < end; index += 1u) {
    if (!ascii_identifier_tail_byte((uint8_t)data[index])) return false;
  }
  return true;
}

static path_check valid_utf8_text(w_seed_frontend_text text) {
  if (text.length != 0u && text.data == NULL) return PATH_CHECK_INVALID;
  w_seed_source source;
  if (!w_seed_source_init(
          (w_seed_byte_view){(const uint8_t *)text.data, text.length},
          &source, NULL))
    return PATH_CHECK_UTF8;
  return PATH_CHECK_OK;
}

static path_check source_id_shape(w_seed_frontend_text source_id,
                                  bool root_source_id) {
  const path_check encoding = valid_utf8_text(source_id);
  if (encoding != PATH_CHECK_OK) return encoding;
  if (source_id.length < 3u || source_id.data == NULL ||
      source_id.data[source_id.length - 2u] != '.' ||
      source_id.data[source_id.length - 1u] != 'w')
    return PATH_CHECK_INVALID;
  if (root_source_id) {
    for (size_t index = 0u; index < source_id.length; index += 1u) {
      if (source_id.data[index] == '/') return PATH_CHECK_INVALID;
    }
  }
  for (size_t index = 0u; index < source_id.length; index += 1u) {
    const uint8_t byte = (uint8_t)source_id.data[index];
    if (byte >= 0x80u) return PATH_CHECK_UNSUPPORTED_NFC;
    if (byte == 0u || byte == (uint8_t)'\\' || byte == (uint8_t)':')
      return PATH_CHECK_INVALID;
  }
  const size_t stem_end = source_id.length - 2u;
  size_t component_start = 0u;
  for (size_t index = 0u; index <= stem_end; index += 1u) {
    if (index != stem_end && source_id.data[index] != '/') continue;
    if (!ascii_identifier_range(source_id.data, component_start, index))
      return PATH_CHECK_INVALID;
    component_start = index + 1u;
  }
  return PATH_CHECK_OK;
}

static path_check root_path_shape(w_seed_byte_view root_path,
                                  size_t maximum) {
  if (root_path.data == NULL || root_path.length == 0u ||
      root_path.length > maximum || contains_nul(root_path.data, root_path.length))
    return PATH_CHECK_INVALID;
  w_seed_source source;
  if (!w_seed_source_init(root_path, &source, NULL)) return PATH_CHECK_UTF8;
  return PATH_CHECK_OK;
}

static bool token_bytes_valid(const char *data, size_t length,
                              size_t capacity, size_t maximum) {
  if (data == NULL || length == 0u || length > capacity ||
      capacity > maximum || contains_nul((const uint8_t *)data, length))
    return false;
  const path_check encoding = valid_utf8_text((w_seed_frontend_text){data, length});
  if (encoding != PATH_CHECK_OK) return false;
  for (size_t index = 0u; index < length; index += 1u) {
    if ((uint8_t)data[index] >= 0x80u) return false;
  }
  return true;
}

static bool token_buffers_shape(
    const w_seed_ephemeral_provider_token_buffers *tokens,
    size_t maximum) {
  if (tokens == NULL) return false;
  return tokens->provider_id != NULL && tokens->provider_id_capacity != 0u &&
         tokens->provider_id_capacity <= maximum &&
         tokens->root_token != NULL && tokens->root_token_capacity != 0u &&
         tokens->root_token_capacity <= maximum &&
         tokens->source_provider_owner_token != NULL &&
         tokens->source_provider_owner_token_capacity != 0u &&
         tokens->source_provider_owner_token_capacity <= maximum &&
         tokens->canonical_token != NULL &&
         tokens->canonical_token_capacity != 0u &&
         tokens->canonical_token_capacity <= maximum;
}

static bool request_basic_shape(
    const w_seed_ephemeral_provider_request *request,
    const w_seed_ephemeral_provider_limits *limits) {
  if (request == NULL || limits == NULL ||
      request->source_id.data == NULL || request->source_id.length == 0u ||
      request->source_id.length > limits->max_path_bytes ||
      request->staging_capacity > W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES ||
      request->revalidation_capacity >
          W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES ||
      request->byte_capacity > W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES ||
      (request->staging_capacity != 0u && request->staging_bytes == NULL) ||
      (request->revalidation_capacity != 0u &&
       request->revalidation_bytes == NULL) ||
      (request->byte_capacity != 0u && request->bytes == NULL) ||
      request->source == NULL || request->facts == NULL ||
      !token_buffers_shape(&request->tokens, limits->max_token_bytes) ||
      !token_buffers_shape(&request->revalidation_tokens,
                           limits->max_token_bytes))
    return false;
  return true;
}

static bool limits_shape(const w_seed_ephemeral_provider_limits *limits) {
  return limits != NULL && limits->max_sources != 0u &&
         limits->max_sources <= W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES &&
         limits->max_source_bytes <=
             W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCE_BYTES &&
         limits->max_total_source_bytes <=
             W_SEED_EPHEMERAL_PROVIDER_MAX_TOTAL_SOURCE_BYTES &&
         limits->max_path_bytes != 0u &&
         limits->max_path_bytes <= W_SEED_EPHEMERAL_PROVIDER_MAX_PATH_BYTES &&
         limits->max_token_bytes != 0u &&
         limits->max_token_bytes <= W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES;
}

static bool backend_shape(const w_seed_ephemeral_provider_backend *backend) {
  return backend != NULL && backend->open_root != NULL &&
         backend->open_source != NULL && backend->read_source != NULL &&
         backend->revalidate_source != NULL && backend->close_source != NULL &&
         backend->close_root != NULL;
}

static bool token_capacity_metadata_shape(
    w_seed_ephemeral_provider_token_capacity capacity) {
  return capacity.required_capacity != 0u &&
         capacity.required_capacity >= capacity.maximum_emitted_length &&
         capacity.maximum_emitted_length != 0u &&
         capacity.required_capacity <=
             W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES &&
         capacity.maximum_emitted_length <=
             W_SEED_EPHEMERAL_PROVIDER_MAX_TOKEN_BYTES;
}

static bool backend_metadata_shape(
    const w_seed_ephemeral_provider_backend *backend) {
  return backend != NULL &&
         token_capacity_metadata_shape(backend->metadata.provider_id) &&
         token_capacity_metadata_shape(backend->metadata.root_token) &&
         token_capacity_metadata_shape(
             backend->metadata.source_provider_owner_token) &&
         token_capacity_metadata_shape(backend->metadata.canonical_token);
}

static bool make_range(const void *data, size_t length, provider_range *range) {
  if (range == NULL) return false;
  range->start = 0u;
  range->end = 0u;
  range->present = false;
  if (length == 0u) return true;
  if (data == NULL) return false;
  const uintptr_t start = (uintptr_t)data;
  if ((uintmax_t)length > (uintmax_t)UINTPTR_MAX - (uintmax_t)start)
    return false;
  range->start = start;
  range->end = start + (uintptr_t)length;
  range->present = true;
  return true;
}

static bool ranges_overlap(provider_range left, provider_range right) {
  return left.present && right.present && left.start < right.end &&
         right.start < left.end;
}

enum { PROVIDER_REQUEST_RANGE_COUNT = 16 };

static bool collect_request_ranges(
    const w_seed_ephemeral_provider_request *request,
    provider_range ranges[PROVIDER_REQUEST_RANGE_COUNT],
    bool require_disjoint) {
  if (request == NULL || ranges == NULL) return false;
  size_t count = 0u;
#define ADD_RANGE(pointer, length)                                               \
  do {                                                                           \
    if (count >= PROVIDER_REQUEST_RANGE_COUNT ||                                \
        !make_range((pointer), (length), &ranges[count]))                       \
      return false;                                                              \
    count += 1u;                                                                 \
  } while (0)
  ADD_RANGE(request->source_id.data, request->source_id.length);
  ADD_RANGE(request->staging_bytes, request->staging_capacity);
  ADD_RANGE(request->revalidation_bytes, request->revalidation_capacity);
  ADD_RANGE(request->bytes, request->byte_capacity);
  ADD_RANGE(request->source, sizeof(*request->source));
  ADD_RANGE(request->facts, sizeof(*request->facts));
  ADD_RANGE(request->tokens.provider_id, request->tokens.provider_id_capacity);
  ADD_RANGE(request->tokens.root_token, request->tokens.root_token_capacity);
  ADD_RANGE(request->tokens.source_provider_owner_token,
            request->tokens.source_provider_owner_token_capacity);
  ADD_RANGE(request->tokens.canonical_token,
            request->tokens.canonical_token_capacity);
  ADD_RANGE(request->revalidation_tokens.provider_id,
            request->revalidation_tokens.provider_id_capacity);
  ADD_RANGE(request->revalidation_tokens.root_token,
            request->revalidation_tokens.root_token_capacity);
  ADD_RANGE(request->revalidation_tokens.source_provider_owner_token,
            request->revalidation_tokens.source_provider_owner_token_capacity);
  ADD_RANGE(request->revalidation_tokens.canonical_token,
            request->revalidation_tokens.canonical_token_capacity);
#undef ADD_RANGE
  while (count < PROVIDER_REQUEST_RANGE_COUNT) {
    ranges[count].start = 0u;
    ranges[count].end = 0u;
    ranges[count].present = false;
    count += 1u;
  }
  if (require_disjoint) {
    for (size_t left = 0u; left < PROVIDER_REQUEST_RANGE_COUNT; left += 1u) {
      for (size_t right = left + 1u; right < PROVIDER_REQUEST_RANGE_COUNT;
           right += 1u) {
        if (ranges_overlap(ranges[left], ranges[right])) return false;
      }
    }
  }
  return true;
}

static bool storage_disjoint(const w_seed_ephemeral_provider_input *input) {
  if (input == NULL || input->requests == NULL) return false;
  provider_range request_array;
  if (input->request_count > SIZE_MAX / sizeof(*input->requests) ||
      !make_range(input->requests,
                  input->request_count * sizeof(*input->requests),
                  &request_array))
    return false;
  provider_range root_path;
  if (!make_range(input->root_path.data, input->root_path.length, &root_path))
    return false;
  provider_range all[W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES]
                     [PROVIDER_REQUEST_RANGE_COUNT];
  for (size_t index = 0u; index < input->request_count; index += 1u) {
    if (!collect_request_ranges(&input->requests[index], all[index], true))
      return false;
    for (size_t range = 0u; range < PROVIDER_REQUEST_RANGE_COUNT; range += 1u) {
      if (ranges_overlap(all[index][range], request_array) ||
          ranges_overlap(all[index][range], root_path))
        return false;
    }
  }
  for (size_t left = 0u; left < input->request_count; left += 1u) {
    for (size_t right = left + 1u; right < input->request_count; right += 1u) {
      for (size_t left_range = 0u; left_range < PROVIDER_REQUEST_RANGE_COUNT;
           left_range += 1u) {
        for (size_t right_range = 0u;
             right_range < PROVIDER_REQUEST_RANGE_COUNT; right_range += 1u) {
          if (ranges_overlap(all[left][left_range], all[right][right_range]))
            return false;
        }
      }
    }
  }
  return true;
}

/* result is an output object. Do not clear it until its storage is proven
 * disjoint from every input range; otherwise an aliased result could corrupt
 * a request before validation starts. An unprovable range is rejected. */
static bool result_overlaps_storage(
    const w_seed_ephemeral_provider_input *input,
    const w_seed_ephemeral_provider_result *result) {
  if (input == NULL || result == NULL) return true;
  provider_range result_range;
  if (!make_range(result, sizeof(*result), &result_range)) return true;
  provider_range input_range;
  if (!make_range(input, sizeof(*input), &input_range) ||
      ranges_overlap(result_range, input_range))
    return true;
  provider_range root_path;
  if (!make_range(input->root_path.data, input->root_path.length, &root_path) ||
      ranges_overlap(result_range, root_path))
    return true;
  if (input->request_count > W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES ||
      input->request_count > SIZE_MAX / sizeof(*input->requests))
    return true;
  provider_range request_array;
  if (!make_range(input->requests,
                  input->request_count * sizeof(*input->requests),
                  &request_array) ||
      ranges_overlap(result_range, request_array))
    return true;
  for (size_t index = 0u; index < input->request_count; index += 1u) {
    provider_range ranges[PROVIDER_REQUEST_RANGE_COUNT];
    if (!collect_request_ranges(&input->requests[index], ranges, false))
      return true;
    for (size_t range = 0u; range < PROVIDER_REQUEST_RANGE_COUNT;
         range += 1u) {
      if (ranges_overlap(result_range, ranges[range])) return true;
    }
  }
  return false;
}

static bool input_shape(const w_seed_ephemeral_provider_input *input) {
  if (input == NULL || !limits_shape(&input->limits) ||
      !backend_shape(&input->backend) ||
      !backend_metadata_shape(&input->backend) || input->requests == NULL ||
      input->request_count == 0u ||
      input->request_count > input->limits.max_sources ||
      input->request_count > W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES ||
      input->root_request_index >= input->request_count ||
      root_path_shape(input->root_path, input->limits.max_path_bytes) !=
          PATH_CHECK_OK ||
      !storage_disjoint(input))
    return false;
  for (size_t index = 0u; index < input->request_count; index += 1u) {
    if (!request_basic_shape(&input->requests[index], &input->limits))
      return false;
    const path_check source_check = source_id_shape(
        input->requests[index].source_id,
        index == input->root_request_index);
    if (source_check != PATH_CHECK_OK) return false;
    for (size_t prior = 0u; prior < index; prior += 1u) {
      if (text_equal(input->requests[index].source_id,
                     input->requests[prior].source_id))
        return false;
    }
  }
  return true;
}

static w_seed_ephemeral_provider_status preflight_token_capacity(
    w_seed_ephemeral_provider_result *result, size_t request_index,
    size_t capacity, w_seed_ephemeral_provider_token_capacity declared,
    w_seed_ephemeral_provider_capacity_field field) {
  if (capacity >= declared.required_capacity)
    return W_SEED_EPHEMERAL_PROVIDER_OK;
  return fail_capacity_result(
      result, W_SEED_EPHEMERAL_PROVIDER_CAPACITY,
      W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT,
      W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE, request_index, field,
      declared.required_capacity, 0u,
      W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
}

static w_seed_ephemeral_provider_status preflight_capacities(
    const w_seed_ephemeral_provider_input *input,
    w_seed_ephemeral_provider_result *result) {
  const w_seed_ephemeral_provider_metadata *metadata =
      &input->backend.metadata;
  for (size_t index = 0u; index < input->request_count; index += 1u) {
    const w_seed_ephemeral_provider_request *request =
        &input->requests[index];
    w_seed_ephemeral_provider_status status = preflight_token_capacity(
        result, index, request->tokens.provider_id_capacity,
        metadata->provider_id,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_PROVIDER_ID);
    if (status != W_SEED_EPHEMERAL_PROVIDER_OK) return status;
    status = preflight_token_capacity(
        result, index, request->tokens.root_token_capacity,
        metadata->root_token,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_ROOT_TOKEN);
    if (status != W_SEED_EPHEMERAL_PROVIDER_OK) return status;
    status = preflight_token_capacity(
        result, index, request->tokens.source_provider_owner_token_capacity,
        metadata->source_provider_owner_token,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_SOURCE_PROVIDER_OWNER_TOKEN);
    if (status != W_SEED_EPHEMERAL_PROVIDER_OK) return status;
    status = preflight_token_capacity(
        result, index, request->tokens.canonical_token_capacity,
        metadata->canonical_token,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_CANONICAL_TOKEN);
    if (status != W_SEED_EPHEMERAL_PROVIDER_OK) return status;
    status = preflight_token_capacity(
        result, index, request->revalidation_tokens.provider_id_capacity,
        metadata->provider_id,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_PROVIDER_ID);
    if (status != W_SEED_EPHEMERAL_PROVIDER_OK) return status;
    status = preflight_token_capacity(
        result, index, request->revalidation_tokens.root_token_capacity,
        metadata->root_token,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_ROOT_TOKEN);
    if (status != W_SEED_EPHEMERAL_PROVIDER_OK) return status;
    status = preflight_token_capacity(
        result,
        index, request->revalidation_tokens.source_provider_owner_token_capacity,
        metadata->source_provider_owner_token,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_SOURCE_PROVIDER_OWNER_TOKEN);
    if (status != W_SEED_EPHEMERAL_PROVIDER_OK) return status;
    status = preflight_token_capacity(
        result, index, request->revalidation_tokens.canonical_token_capacity,
        metadata->canonical_token,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_CANONICAL_TOKEN);
    if (status != W_SEED_EPHEMERAL_PROVIDER_OK) return status;
  }
  return W_SEED_EPHEMERAL_PROVIDER_OK;
}

static w_seed_ephemeral_provider_failure input_failure(
    const w_seed_ephemeral_provider_input *input) {
  if (input == NULL || input->requests == NULL ||
      input->root_path.data == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_FAILURE_POINTER;
  if (!limits_shape(&input->limits) ||
      input->request_count > W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES ||
      input->request_count > input->limits.max_sources)
    return W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT;
  if (input->root_request_index >= input->request_count)
    return W_SEED_EPHEMERAL_PROVIDER_FAILURE_ORDER;
  if (root_path_shape(input->root_path, input->limits.max_path_bytes) !=
      PATH_CHECK_OK) {
    if (root_path_shape(input->root_path, input->limits.max_path_bytes) ==
        PATH_CHECK_UTF8)
      return W_SEED_EPHEMERAL_PROVIDER_FAILURE_INVALID_UTF8;
    return W_SEED_EPHEMERAL_PROVIDER_FAILURE_PATH;
  }
  if (!backend_shape(&input->backend))
    return W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND;
  if (!backend_metadata_shape(&input->backend))
    return W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND;
  if (!storage_disjoint(input)) return W_SEED_EPHEMERAL_PROVIDER_FAILURE_POINTER;
  for (size_t index = 0u; index < input->request_count; index += 1u) {
    if (!request_basic_shape(&input->requests[index], &input->limits))
      return W_SEED_EPHEMERAL_PROVIDER_FAILURE_POINTER;
    const path_check check = source_id_shape(
        input->requests[index].source_id,
        index == input->root_request_index);
    if (check == PATH_CHECK_UTF8)
      return W_SEED_EPHEMERAL_PROVIDER_FAILURE_INVALID_UTF8;
    if (check == PATH_CHECK_UNSUPPORTED_NFC)
      return W_SEED_EPHEMERAL_PROVIDER_FAILURE_UNSUPPORTED_NFC;
    if (check != PATH_CHECK_OK)
      return W_SEED_EPHEMERAL_PROVIDER_FAILURE_PATH;
    for (size_t prior = 0u; prior < index; prior += 1u) {
      if (text_equal(input->requests[index].source_id,
                     input->requests[prior].source_id))
        return W_SEED_EPHEMERAL_PROVIDER_FAILURE_ORDER;
    }
  }
  return W_SEED_EPHEMERAL_PROVIDER_FAILURE_POINTER;
}

static w_seed_frontend_text token_view(const char *data, size_t length) {
  return (w_seed_frontend_text){data, length};
}

static bool observation_shape(
    const w_seed_ephemeral_provider_observation *observation,
    const w_seed_ephemeral_provider_token_buffers *tokens,
    const w_seed_ephemeral_provider_metadata *metadata, size_t maximum) {
  if (observation == NULL || !observation->opened ||
      !observation->containment_inside ||
      (observation->symlink != W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE &&
       observation->symlink != W_SEED_EPHEMERAL_GRAPH_SYMLINK_INSIDE) ||
      !token_buffers_shape(tokens, maximum) || metadata == NULL)
    return false;
  return token_bytes_valid(tokens->provider_id, observation->provider_id_length,
                           tokens->provider_id_capacity, maximum) &&
         observation->provider_id_length <=
             metadata->provider_id.maximum_emitted_length &&
         token_bytes_valid(tokens->root_token, observation->root_token_length,
                           tokens->root_token_capacity, maximum) &&
         observation->root_token_length <=
             metadata->root_token.maximum_emitted_length &&
         token_bytes_valid(tokens->source_provider_owner_token,
                           observation->source_provider_owner_token_length,
                           tokens->source_provider_owner_token_capacity,
                           maximum) &&
         observation->source_provider_owner_token_length <=
             metadata->source_provider_owner_token.maximum_emitted_length &&
         token_bytes_valid(tokens->canonical_token,
                           observation->canonical_token_length,
                           tokens->canonical_token_capacity, maximum) &&
         observation->canonical_token_length <=
             metadata->canonical_token.maximum_emitted_length;
}

static w_seed_ephemeral_provider_failure observation_failure(
    const w_seed_ephemeral_provider_observation *observation) {
  if (observation == NULL ||
      (observation->symlink != W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE &&
       observation->symlink != W_SEED_EPHEMERAL_GRAPH_SYMLINK_INSIDE))
    return W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK;
  if (observation == NULL || !observation->containment_inside)
    return W_SEED_EPHEMERAL_PROVIDER_FAILURE_CONTAINMENT;
  return W_SEED_EPHEMERAL_PROVIDER_FAILURE_TOKEN;
}

static w_seed_frontend_text observation_provider_id(
    const w_seed_ephemeral_provider_observation *observation,
    const w_seed_ephemeral_provider_token_buffers *tokens) {
  return token_view(tokens->provider_id, observation->provider_id_length);
}

static w_seed_frontend_text observation_root_token(
    const w_seed_ephemeral_provider_observation *observation,
    const w_seed_ephemeral_provider_token_buffers *tokens) {
  return token_view(tokens->root_token, observation->root_token_length);
}

static w_seed_frontend_text observation_owner_token(
    const w_seed_ephemeral_provider_observation *observation,
    const w_seed_ephemeral_provider_token_buffers *tokens) {
  return token_view(tokens->source_provider_owner_token,
                   observation->source_provider_owner_token_length);
}

static w_seed_frontend_text observation_canonical_token(
    const w_seed_ephemeral_provider_observation *observation,
    const w_seed_ephemeral_provider_token_buffers *tokens) {
  return token_view(tokens->canonical_token, observation->canonical_token_length);
}

static bool observation_identity_matches(
    const w_seed_ephemeral_provider_observation *observation,
    const w_seed_ephemeral_provider_token_buffers *tokens,
    const w_seed_ephemeral_graph_provider_facts *facts) {
  return text_equal(observation_provider_id(observation, tokens),
                    facts->provider_id) &&
         text_equal(observation_root_token(observation, tokens),
                    facts->root_token) &&
         text_equal(observation_owner_token(observation, tokens),
                    facts->source_provider_owner_token) &&
         text_equal(observation_canonical_token(observation, tokens),
                    facts->canonical_token);
}

static bool same_provider_identity(
    const w_seed_ephemeral_provider_observation *observation,
    const w_seed_ephemeral_provider_token_buffers *tokens,
    const w_seed_ephemeral_graph_provider_facts *root_facts) {
  return text_equal(observation_provider_id(observation, tokens),
                    root_facts->provider_id) &&
         text_equal(observation_root_token(observation, tokens),
                    root_facts->root_token) &&
         text_equal(observation_owner_token(observation, tokens),
                    root_facts->source_provider_owner_token);
}

static void set_initial_facts(
    w_seed_ephemeral_graph_provider_facts *facts,
    const w_seed_ephemeral_provider_observation *observation,
    const w_seed_ephemeral_provider_token_buffers *tokens, size_t byte_count,
    const uint8_t digest[32]) {
  (void)memset(facts, 0, sizeof(*facts));
  facts->provider_id = observation_provider_id(observation, tokens);
  facts->root_token = observation_root_token(observation, tokens);
  facts->source_provider_owner_token = observation_owner_token(observation, tokens);
  facts->canonical_token = observation_canonical_token(observation, tokens);
  facts->opened = observation->opened;
  facts->containment_inside = observation->containment_inside;
  facts->symlink = observation->symlink;
  facts->snapshot_before_byte_count = byte_count;
  facts->snapshot_after_byte_count = byte_count;
  (void)memcpy(facts->snapshot_before_digest, digest, 32u);
}

static void set_after_facts(
    w_seed_ephemeral_graph_provider_facts *facts, size_t byte_count,
    const uint8_t digest[32]) {
  facts->snapshot_after_byte_count = byte_count;
  (void)memcpy(facts->snapshot_after_digest, digest, 32u);
}

static void clear_revalidation_tokens(
    w_seed_ephemeral_provider_token_buffers *tokens) {
  if (tokens == NULL) return;
  if (tokens->provider_id != NULL && tokens->provider_id_capacity != 0u)
    (void)memset(tokens->provider_id, 0, tokens->provider_id_capacity);
  if (tokens->root_token != NULL && tokens->root_token_capacity != 0u)
    (void)memset(tokens->root_token, 0, tokens->root_token_capacity);
  if (tokens->source_provider_owner_token != NULL &&
      tokens->source_provider_owner_token_capacity != 0u)
    (void)memset(tokens->source_provider_owner_token, 0,
                 tokens->source_provider_owner_token_capacity);
  if (tokens->canonical_token != NULL && tokens->canonical_token_capacity != 0u)
    (void)memset(tokens->canonical_token, 0, tokens->canonical_token_capacity);
}

static w_seed_ephemeral_provider_status map_backend_failure(
    w_seed_ephemeral_provider_result *result,
    w_seed_ephemeral_provider_phase phase, size_t request_index,
    w_seed_ephemeral_provider_backend_status backend_status,
    w_seed_ephemeral_provider_failure io_failure,
    size_t required_byte_capacity, size_t observed_byte_count) {
  switch (backend_status) {
    case W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY:
      /* Capacity is legal only for the byte callbacks, which validate the
       * reported required size before reaching this mapper. An open callback
       * has already passed token-capacity preflight, so CAPACITY there is a
       * backend contract failure rather than an estimate. */
      return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                         W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND, phase,
                         request_index, 0u, observed_byte_count,
                         backend_status);
    case W_SEED_EPHEMERAL_PROVIDER_BACKEND_UNSUPPORTED:
      return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_UNSUPPORTED,
                         W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND, phase,
                         request_index, required_byte_capacity,
                         observed_byte_count, backend_status);
    case W_SEED_EPHEMERAL_PROVIDER_BACKEND_ESCAPE:
      return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                         W_SEED_EPHEMERAL_PROVIDER_FAILURE_CONTAINMENT, phase,
                         request_index, required_byte_capacity,
                         observed_byte_count, backend_status);
    case W_SEED_EPHEMERAL_PROVIDER_BACKEND_SYMLINK:
      return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                         W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK, phase,
                         request_index, required_byte_capacity,
                         observed_byte_count, backend_status);
    case W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND:
      return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_IO, io_failure, phase,
                         request_index, required_byte_capacity,
                         observed_byte_count, backend_status);
    case W_SEED_EPHEMERAL_PROVIDER_BACKEND_IO:
      return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_IO, io_failure, phase,
                         request_index, required_byte_capacity,
                         observed_byte_count, backend_status);
    case W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID:
      return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                         W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND, phase,
                         request_index, required_byte_capacity,
                         observed_byte_count, backend_status);
    case W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK:
      break;
  }
  return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                     W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND, phase,
                     request_index, required_byte_capacity,
                     observed_byte_count, backend_status);
}

static void close_plan(const w_seed_ephemeral_provider_input *input,
                       provider_plan *plan) {
  if (input == NULL || plan == NULL) return;
  for (size_t offset = input->request_count; offset != 0u; offset -= 1u) {
    const size_t index = offset - 1u;
    if (!plan->source_open[index]) continue;
    input->backend.close_source(input->backend.context,
                                plan->source_handles[index]);
    plan->source_open[index] = false;
  }
  if (plan->root_open) {
    input->backend.close_root(input->backend.context, plan->root_handle);
    plan->root_open = false;
  }
}

static bool handle_is_open(const provider_plan *plan, size_t request_count,
                           w_seed_ephemeral_provider_handle handle) {
  if (plan == NULL || handle.value == (uintptr_t)0u) return false;
  if (plan->root_open && plan->root_handle.value == handle.value) return true;
  for (size_t index = 0u; index < request_count; index += 1u) {
    if (plan->source_open[index] &&
        plan->source_handles[index].value == handle.value)
      return true;
  }
  return false;
}

static w_seed_ephemeral_provider_status abort_plan(
    const w_seed_ephemeral_provider_input *input, provider_plan *plan,
    w_seed_ephemeral_provider_result *result,
    w_seed_ephemeral_provider_status status) {
  if (result != NULL) result->total_source_bytes = plan->total_source_bytes;
  close_plan(input, plan);
  return status;
}

static bool canonical_is_unique(
    const provider_plan *plan, size_t index, w_seed_frontend_text canonical) {
  for (size_t prior = 0u; prior < index; prior += 1u) {
    if (!plan->source_open[prior]) continue;
    if (text_equal(plan->facts[prior].canonical_token, canonical)) return false;
  }
  for (size_t later = index + 1u;
       later < W_SEED_EPHEMERAL_PROVIDER_MAX_SOURCES; later += 1u) {
    if (plan->source_open[later] &&
        text_equal(plan->facts[later].canonical_token, canonical))
      return false;
  }
  return true;
}

static w_seed_ephemeral_provider_status acquire_one(
    const w_seed_ephemeral_provider_input *input, provider_plan *plan,
    w_seed_ephemeral_provider_result *result, size_t index) {
  const w_seed_ephemeral_provider_request *request = &input->requests[index];
  size_t written = 0u;
  const w_seed_ephemeral_provider_backend_status backend_status =
      input->backend.read_source(input->backend.context,
                                 plan->source_handles[index],
                                 request->staging_bytes,
                                 request->staging_capacity, &written);
  if (backend_status == W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY) {
    if (written <= request->staging_capacity)
      return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                         W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND,
                         W_SEED_EPHEMERAL_PROVIDER_PHASE_READ, index, 0u,
                         written, backend_status);
    return fail_capacity_result(
        result, W_SEED_EPHEMERAL_PROVIDER_CAPACITY,
        W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT,
        W_SEED_EPHEMERAL_PROVIDER_PHASE_READ, index,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, written,
        written, backend_status);
  }
  if (backend_status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
    return map_backend_failure(
        result, W_SEED_EPHEMERAL_PROVIDER_PHASE_READ, index, backend_status,
        W_SEED_EPHEMERAL_PROVIDER_FAILURE_SOURCE_READ, 0u, written);
  }
  if (written > request->staging_capacity) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_READ, index, written,
                       written, W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  if (written > input->limits.max_source_bytes) {
    return fail_capacity_result(
        result, W_SEED_EPHEMERAL_PROVIDER_CAPACITY,
        W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT,
        W_SEED_EPHEMERAL_PROVIDER_PHASE_READ, index,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_STAGING_BYTES, written,
        written, W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  if (plan->total_source_bytes > input->limits.max_total_source_bytes ||
      written > input->limits.max_total_source_bytes -
                    plan->total_source_bytes) {
    return fail_capacity_result(
        result, W_SEED_EPHEMERAL_PROVIDER_CAPACITY,
        W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT,
        W_SEED_EPHEMERAL_PROVIDER_PHASE_READ, index,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_AGGREGATE_SOURCE_BYTES,
        plan->total_source_bytes + written, written,
        W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  w_seed_source_error source_error;
  if (!w_seed_source_init(
          (w_seed_byte_view){request->staging_bytes, written},
          &plan->sources[index], &source_error)) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_SOURCE_ENCODING,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_READ, index, 0u, written,
                       W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  w_seed_ephemeral_graph_source_digest(&plan->sources[index],
                                       plan->before_digest[index]);
  plan->byte_counts[index] = written;
  plan->total_source_bytes += written;
  return W_SEED_EPHEMERAL_PROVIDER_OK;
}

static w_seed_ephemeral_provider_status open_root(
    const w_seed_ephemeral_provider_input *input, provider_plan *plan,
    w_seed_ephemeral_provider_result *result) {
  const size_t index = input->root_request_index;
  const w_seed_ephemeral_provider_request *request = &input->requests[index];
  w_seed_ephemeral_provider_observation observation = {0};
  plan->root_handle.value = (uintptr_t)0u;
  plan->source_handles[index].value = (uintptr_t)0u;
  const w_seed_ephemeral_provider_backend_status backend_status =
      input->backend.open_root(
          input->backend.context, input->root_path,
          (w_seed_ephemeral_provider_token_buffers *)&request->tokens,
          &plan->root_handle, &plan->source_handles[index], &observation);
  if (backend_status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
    return map_backend_failure(
        result, W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT, index,
        backend_status, W_SEED_EPHEMERAL_PROVIDER_FAILURE_ROOT, 0u, 0u);
  }
  const bool root_handle_valid = plan->root_handle.value != (uintptr_t)0u;
  const bool source_handle_valid =
      plan->source_handles[index].value != (uintptr_t)0u;
  if (!root_handle_valid || !source_handle_valid ||
      plan->root_handle.value == plan->source_handles[index].value) {
    /* A malformed backend may return one handle twice. Close that one
     * ownership exactly once through the source close path. */
    plan->root_open = root_handle_valid &&
                      (!source_handle_valid ||
                       plan->root_handle.value !=
                           plan->source_handles[index].value);
    plan->source_open[index] = source_handle_valid;
    return fail_result(
        result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
        W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND,
        W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT, index, 0u, 0u,
        W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  plan->root_open = true;
  plan->source_open[index] = true;
  if (!observation_shape(&observation, &request->tokens,
                         &input->backend.metadata,
                         input->limits.max_token_bytes)) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       observation_failure(&observation),
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT, index, 0u, 0u,
                       W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  if (!canonical_is_unique(
          plan, index,
          observation_canonical_token(&observation, &request->tokens))) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_CANONICAL_ALIAS,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT, index, 0u, 0u,
                       W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  plan->observations[index] = observation;
  return W_SEED_EPHEMERAL_PROVIDER_OK;
}

static w_seed_ephemeral_provider_status open_child(
    const w_seed_ephemeral_provider_input *input, provider_plan *plan,
    w_seed_ephemeral_provider_result *result, size_t index,
    const w_seed_ephemeral_graph_provider_facts *root_facts) {
  const w_seed_ephemeral_provider_request *request = &input->requests[index];
  w_seed_ephemeral_provider_observation observation = {0};
  plan->source_handles[index].value = (uintptr_t)0u;
  const w_seed_ephemeral_provider_backend_status backend_status =
      input->backend.open_source(
          input->backend.context, plan->root_handle, request->source_id,
          (w_seed_ephemeral_provider_token_buffers *)&request->tokens,
          &plan->source_handles[index], &observation);
  if (backend_status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
    return map_backend_failure(
        result, W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE, index,
        backend_status, W_SEED_EPHEMERAL_PROVIDER_FAILURE_SOURCE_MISSING, 0u,
        0u);
  }
  if (plan->source_handles[index].value == (uintptr_t)0u ||
      handle_is_open(plan, input->request_count,
                     plan->source_handles[index])) {
    return fail_result(
        result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
        W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND,
        W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE, index, 0u, 0u,
        W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  plan->source_open[index] = true;
  if (!observation_shape(&observation, &request->tokens,
                         &input->backend.metadata,
                         input->limits.max_token_bytes)) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       observation_failure(&observation),
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE, index, 0u,
                       0u, W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  if (!same_provider_identity(&observation, &request->tokens, root_facts)) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_TOKEN,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE, index, 0u,
                       0u, W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  if (!canonical_is_unique(
          plan, index,
          observation_canonical_token(&observation, &request->tokens))) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_CANONICAL_ALIAS,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE, index, 0u,
                       0u, W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  plan->observations[index] = observation;
  return W_SEED_EPHEMERAL_PROVIDER_OK;
}

static w_seed_ephemeral_provider_status revalidate_one(
    const w_seed_ephemeral_provider_input *input, provider_plan *plan,
    w_seed_ephemeral_provider_result *result, size_t index) {
  const w_seed_ephemeral_provider_request *request = &input->requests[index];
  clear_revalidation_tokens(
      (w_seed_ephemeral_provider_token_buffers *)&request->revalidation_tokens);
  w_seed_ephemeral_provider_observation observation = {0};
  size_t written = 0u;
  const w_seed_ephemeral_provider_backend_status backend_status =
      input->backend.revalidate_source(
          input->backend.context, plan->root_handle,
          plan->source_handles[index], request->source_id,
          (w_seed_ephemeral_provider_token_buffers *)&request->revalidation_tokens,
          &observation, request->revalidation_bytes,
          request->revalidation_capacity, &written);
  if (backend_status == W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY) {
    if (written <= request->revalidation_capacity)
      return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                         W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND,
                         W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE, index,
                         0u, written, backend_status);
    return fail_capacity_result(
        result, W_SEED_EPHEMERAL_PROVIDER_CAPACITY,
        W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT,
        W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE, index,
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_BYTES, written,
        written, backend_status);
  }
  if (backend_status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) {
    return map_backend_failure(
        result, W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE, index,
        backend_status, W_SEED_EPHEMERAL_PROVIDER_FAILURE_SNAPSHOT, 0u,
        written);
  }
  if (written > request->revalidation_capacity) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE, index,
                       written, written,
                       W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  if (!observation_shape(&observation, &request->revalidation_tokens,
                         &input->backend.metadata,
                         input->limits.max_token_bytes)) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_SNAPSHOT,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE, index, 0u,
                       written, W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  if (!observation_identity_matches(
          &observation, &request->revalidation_tokens, &plan->facts[index]) ||
      observation.symlink != plan->observations[index].symlink ||
      written != plan->byte_counts[index] ||
      (written != 0u &&
       memcmp(request->staging_bytes, request->revalidation_bytes, written) !=
           0)) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_SNAPSHOT,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE, index, 0u,
                       written, W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  w_seed_source revalidated_source;
  if (!w_seed_source_init(
          (w_seed_byte_view){request->revalidation_bytes, written},
          &revalidated_source, NULL)) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_SOURCE_ENCODING,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE, index, 0u,
                       written, W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  w_seed_ephemeral_graph_source_digest(&revalidated_source,
                                       plan->after_digest[index]);
  if (memcmp(plan->before_digest[index], plan->after_digest[index], 32u) != 0) {
    return fail_result(result, W_SEED_EPHEMERAL_PROVIDER_INVALID,
                       W_SEED_EPHEMERAL_PROVIDER_FAILURE_SNAPSHOT,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_REVALIDATE, index, 0u,
                       written, W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  set_after_facts(&plan->facts[index], written, plan->after_digest[index]);
  return W_SEED_EPHEMERAL_PROVIDER_OK;
}

static void commit_plan(const w_seed_ephemeral_provider_input *input,
                        const provider_plan *plan) {
  for (size_t index = 0u; index < input->request_count; index += 1u) {
    const w_seed_ephemeral_provider_request *request = &input->requests[index];
    const size_t byte_count = plan->byte_counts[index];
    if (byte_count != 0u)
      (void)memcpy(request->bytes, request->staging_bytes, byte_count);
    w_seed_source published_source = plan->sources[index];
    published_source.bytes.data = request->bytes;
    published_source.bytes.length = byte_count;
    *request->source = published_source;
    *request->facts = plan->facts[index];
  }
}

w_seed_ephemeral_provider_status w_seed_ephemeral_provider_acquire(
    const w_seed_ephemeral_provider_input *input,
    w_seed_ephemeral_provider_result *result) {
  if (result == NULL) return W_SEED_EPHEMERAL_PROVIDER_INVALID;
  if (input == NULL) {
    clear_result(result);
    result->status = W_SEED_EPHEMERAL_PROVIDER_INVALID;
    result->failure = W_SEED_EPHEMERAL_PROVIDER_FAILURE_POINTER;
    return W_SEED_EPHEMERAL_PROVIDER_INVALID;
  }
  if (result_overlaps_storage(input, result))
    return W_SEED_EPHEMERAL_PROVIDER_INVALID;
  clear_result(result);
  if (!input_shape(input)) {
    const w_seed_ephemeral_provider_failure failure = input_failure(input);
    const w_seed_ephemeral_provider_status status =
        failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_UNSUPPORTED_NFC
            ? W_SEED_EPHEMERAL_PROVIDER_UNSUPPORTED
            : W_SEED_EPHEMERAL_PROVIDER_INVALID;
    return fail_result(result, status, failure,
                       W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE, SIZE_MAX, 0u,
                       0u, W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
  }
  w_seed_ephemeral_provider_status status =
      preflight_capacities(input, result);
  if (status != W_SEED_EPHEMERAL_PROVIDER_OK) return status;
  provider_plan plan;
  (void)memset(&plan, 0, sizeof(plan));
  const size_t root_index = input->root_request_index;
  status = open_root(input, &plan, result);
  if (status != W_SEED_EPHEMERAL_PROVIDER_OK) {
    return abort_plan(input, &plan, result, status);
  }
  const w_seed_ephemeral_provider_request *root_request =
      &input->requests[root_index];
  status = acquire_one(input, &plan, result, root_index);
  if (status != W_SEED_EPHEMERAL_PROVIDER_OK) {
    return abort_plan(input, &plan, result, status);
  }
  set_initial_facts(&plan.facts[root_index], &plan.observations[root_index],
                    &root_request->tokens, plan.byte_counts[root_index],
                    plan.before_digest[root_index]);
  plan.facts[root_index].snapshot_before_byte_count =
      plan.byte_counts[root_index];
  for (size_t index = 0u; index < input->request_count; index += 1u) {
    if (index == root_index) continue;
    status = open_child(input, &plan, result, index, &plan.facts[root_index]);
    if (status != W_SEED_EPHEMERAL_PROVIDER_OK) {
      return abort_plan(input, &plan, result, status);
    }
    const w_seed_ephemeral_provider_request *request = &input->requests[index];
    status = acquire_one(input, &plan, result, index);
    if (status != W_SEED_EPHEMERAL_PROVIDER_OK) {
      return abort_plan(input, &plan, result, status);
    }
    set_initial_facts(&plan.facts[index], &plan.observations[index],
                      &request->tokens, plan.byte_counts[index],
                      plan.before_digest[index]);
    plan.facts[index].snapshot_before_byte_count = plan.byte_counts[index];
  }
  for (size_t index = 0u; index < input->request_count; index += 1u) {
    const size_t byte_count = plan.byte_counts[index];
    if (input->requests[index].byte_capacity < byte_count) {
      status = fail_capacity_result(
          result, W_SEED_EPHEMERAL_PROVIDER_CAPACITY,
          W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT,
          W_SEED_EPHEMERAL_PROVIDER_PHASE_COMMIT, index,
          W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_OUTPUT_BYTES, byte_count,
          byte_count, W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK);
      return abort_plan(input, &plan, result, status);
    }
  }
  for (size_t index = 0u; index < input->request_count; index += 1u) {
    status = revalidate_one(input, &plan, result, index);
    if (status != W_SEED_EPHEMERAL_PROVIDER_OK) {
      return abort_plan(input, &plan, result, status);
    }
  }
  commit_plan(input, &plan);
  close_plan(input, &plan);
  result->status = W_SEED_EPHEMERAL_PROVIDER_OK;
  result->failure = W_SEED_EPHEMERAL_PROVIDER_FAILURE_NONE;
  result->phase = W_SEED_EPHEMERAL_PROVIDER_PHASE_COMMIT;
  result->request_index = SIZE_MAX;
  result->total_source_bytes = plan.total_source_bytes;
  result->required_byte_capacity = 0u;
  result->observed_byte_count = 0u;
  result->backend_status = W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
  return W_SEED_EPHEMERAL_PROVIDER_OK;
}
