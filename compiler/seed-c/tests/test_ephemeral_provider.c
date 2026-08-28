#include "w_seed_ephemeral_provider.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      (void)fprintf(stderr, "ephemeral provider check failed: %s (%s:%d)\n", \
                    #condition, __FILE__, __LINE__);                            \
      return false;                                                             \
    }                                                                           \
  } while (0)

enum {
  TEST_BYTE_CAPACITY = 64,
  TEST_TOKEN_CAPACITY = 32,
};

/* Independently derived with SHA-256(tag || "child\n"). */
static const uint8_t expected_child_digest[
    W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES] = {
    0xE9u, 0x00u, 0x3Au, 0x69u, 0xE1u, 0x0Eu, 0xB4u, 0x7Au,
    0x94u, 0x3Eu, 0xC4u, 0x51u, 0xFBu, 0xA1u, 0x5Cu, 0x0Au,
    0xDEu, 0x31u, 0x9Eu, 0xBEu, 0x51u, 0xEAu, 0xE1u, 0x2Du,
    0xBDu, 0xB5u, 0xC8u, 0x58u, 0x28u, 0x34u, 0x96u, 0x62u};

typedef struct {
  const char *source_id;
  const uint8_t *before;
  size_t before_length;
  const uint8_t *after;
  size_t after_length;
  const char *canonical;
} fake_file;

typedef struct {
  fake_file files[2];
  bool root_open;
  bool missing;
  bool escape;
  bool symlink_escape;
  bool symlink_unproven;
  bool alias;
  bool mutate;
  bool grow;
  bool provider_drift;
  bool revalidation_drift;
  bool forged_lengths;
  bool token_nul;
  bool written_over_capacity;
  bool reject_revalidation;
  bool zero_root_handle;
  bool zero_root_source_handle;
  bool duplicate_root_handles;
  bool duplicate_child_handle;
  size_t open_root_calls;
  size_t open_source_calls;
  size_t read_calls;
  size_t revalidation_calls;
  size_t close_source_invocations;
  size_t close_source_calls;
  size_t close_source_handle1_calls;
  size_t close_source_handle2_calls;
  size_t close_root_invocations;
  size_t close_root_calls;
  char events[64];
  size_t event_count;
} fake_backend;

typedef struct {
  char source_id[32];
  uint8_t staging[TEST_BYTE_CAPACITY];
  uint8_t revalidation[TEST_BYTE_CAPACITY];
  uint8_t bytes[TEST_BYTE_CAPACITY];
  w_seed_source source;
  w_seed_ephemeral_graph_provider_facts facts;
  char provider[TEST_TOKEN_CAPACITY];
  char root_token[TEST_TOKEN_CAPACITY];
  char owner[TEST_TOKEN_CAPACITY];
  char canonical[TEST_TOKEN_CAPACITY];
  char revalidation_provider[TEST_TOKEN_CAPACITY];
  char revalidation_root_token[TEST_TOKEN_CAPACITY];
  char revalidation_owner[TEST_TOKEN_CAPACITY];
  char revalidation_canonical[TEST_TOKEN_CAPACITY];
  w_seed_ephemeral_provider_request request;
} provider_slot;

typedef struct {
  char root_path[64];
  fake_backend backend;
  provider_slot slots[2];
  w_seed_ephemeral_provider_request requests[2];
  w_seed_ephemeral_provider_input input;
} provider_fixture;

static void record_event(fake_backend *backend, char event) {
  if (backend->event_count < sizeof(backend->events)) {
    backend->events[backend->event_count] = event;
    backend->event_count += 1u;
  }
}

static bool copy_token(char *destination, size_t capacity, const char *source,
                       size_t length) {
  if (destination == NULL || source == NULL || length > capacity) return false;
  if (length != 0u) (void)memcpy(destination, source, length);
  return true;
}

static fake_file *file_for_handle(fake_backend *backend,
                                  w_seed_ephemeral_provider_handle handle) {
  if (handle.value == (uintptr_t)1u) return &backend->files[0];
  if (handle.value == (uintptr_t)2u) return &backend->files[1];
  return NULL;
}

static bool file_matches(const fake_file *file, w_seed_frontend_text source_id) {
  const size_t length = strlen(file->source_id);
  return source_id.length == length &&
         memcmp(source_id.data, file->source_id, length) == 0;
}

static w_seed_ephemeral_provider_backend_status fill_observation(
    fake_backend *backend, const fake_file *file,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation, bool revalidation) {
  const char provider[] = "provider";
  const char root[] = "root";
  const char owner[] = "owner";
  const char drift[] = "drift";
  const char canonical_alias[] = "canonical-root";
  const char *provider_value =
      (revalidation && backend->revalidation_drift) ||
              (!revalidation && backend->provider_drift &&
               file == &backend->files[1])
          ? drift
          : provider;
  const char *canonical_value =
      backend->alias && file == &backend->files[1]
          ? canonical_alias
          : file->canonical;
  if (backend->token_nul && !revalidation) {
    static const char nul_token[] = {'p', 'r', 'o', 0, 'v', 'i', 'd', 'e', 'r'};
    if (!copy_token(tokens->provider_id, tokens->provider_id_capacity,
                    nul_token, sizeof(nul_token)))
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    observation->provider_id_length = sizeof(nul_token);
  } else {
    if (!copy_token(tokens->provider_id, tokens->provider_id_capacity,
                    provider_value, strlen(provider_value)) ||
        !copy_token(tokens->root_token, tokens->root_token_capacity, root,
                    sizeof(root) - 1u) ||
        !copy_token(tokens->source_provider_owner_token,
                    tokens->source_provider_owner_token_capacity, owner,
                    sizeof(owner) - 1u) ||
        !copy_token(tokens->canonical_token, tokens->canonical_token_capacity,
                    canonical_value, strlen(canonical_value)))
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
    observation->provider_id_length = strlen(provider_value);
  }
  if (backend->token_nul && !revalidation) {
    if (!copy_token(tokens->root_token, tokens->root_token_capacity, root,
                    sizeof(root) - 1u) ||
        !copy_token(tokens->source_provider_owner_token,
                    tokens->source_provider_owner_token_capacity, owner,
                    sizeof(owner) - 1u) ||
        !copy_token(tokens->canonical_token, tokens->canonical_token_capacity,
                    canonical_value, strlen(canonical_value)))
      return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  }
  observation->root_token_length = sizeof(root) - 1u;
  observation->source_provider_owner_token_length = sizeof(owner) - 1u;
  observation->canonical_token_length = strlen(canonical_value);
  if (backend->forged_lengths) observation->canonical_token_length += 100u;
  observation->opened = true;
  observation->containment_inside = true;
  observation->symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_NONE;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_backend_status fake_open_root(
    void *context, w_seed_byte_view root_path,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *root_handle,
    w_seed_ephemeral_provider_handle *root_source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  fake_backend *backend = context;
  if (backend == NULL || root_path.data == NULL || root_path.length == 0u ||
      root_handle == NULL || root_source_handle == NULL || observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  record_event(backend, 'R');
  backend->open_root_calls += 1u;
  backend->root_open = true;
  root_handle->value = (uintptr_t)99u;
  root_source_handle->value = (uintptr_t)1u;
  if (backend->zero_root_handle) root_handle->value = (uintptr_t)0u;
  if (backend->zero_root_source_handle)
    root_source_handle->value = (uintptr_t)0u;
  if (backend->duplicate_root_handles)
    root_source_handle->value = root_handle->value;
  if (backend->zero_root_handle && backend->zero_root_source_handle) {
    root_handle->value = (uintptr_t)0u;
    root_source_handle->value = (uintptr_t)0u;
  }
  (void)memset(observation, 0, sizeof(*observation));
  return fill_observation(backend, &backend->files[0], tokens, observation,
                          false);
}

static w_seed_ephemeral_provider_backend_status fake_open_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_handle *source_handle,
    w_seed_ephemeral_provider_observation *observation) {
  fake_backend *backend = context;
  if (backend == NULL || !backend->root_open || root_handle.value != (uintptr_t)99u ||
      source_handle == NULL || observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  record_event(backend, 'O');
  backend->open_source_calls += 1u;
  if (backend->missing || !file_matches(&backend->files[1], source_id))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_NOT_FOUND;
  if (backend->escape) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_ESCAPE;
  source_handle->value = (uintptr_t)2u;
  if (backend->duplicate_child_handle)
    source_handle->value = (uintptr_t)99u;
  (void)memset(observation, 0, sizeof(*observation));
  const w_seed_ephemeral_provider_backend_status status = fill_observation(
      backend, &backend->files[1], tokens, observation, false);
  if (status != W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK) return status;
  if (backend->symlink_escape)
    observation->symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_ESCAPE;
  if (backend->symlink_unproven)
    observation->symlink = W_SEED_EPHEMERAL_GRAPH_SYMLINK_UNPROVEN;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_backend_status fake_read_source(
    void *context, w_seed_ephemeral_provider_handle source_handle,
    uint8_t *bytes, size_t capacity, size_t *written) {
  fake_backend *backend = context;
  if (backend == NULL || written == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  fake_file *file = file_for_handle(backend, source_handle);
  if (file == NULL) return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  record_event(backend, 'r');
  backend->read_calls += 1u;
  if (file->before_length > capacity) {
    *written = file->before_length;
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY;
  }
  if (file->before_length != 0u && bytes == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  if (file->before_length != 0u)
    (void)memcpy(bytes, file->before, file->before_length);
  *written = file->before_length;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static w_seed_ephemeral_provider_backend_status fake_revalidate_source(
    void *context, w_seed_ephemeral_provider_handle root_handle,
    w_seed_ephemeral_provider_handle source_handle,
    w_seed_frontend_text source_id,
    w_seed_ephemeral_provider_token_buffers *tokens,
    w_seed_ephemeral_provider_observation *observation, uint8_t *bytes,
    size_t capacity, size_t *written) {
  fake_backend *backend = context;
  if (backend == NULL || !backend->root_open || root_handle.value != (uintptr_t)99u ||
      written == NULL || observation == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  record_event(backend, 'V');
  backend->revalidation_calls += 1u;
  if (backend->reject_revalidation)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  fake_file *file = file_for_handle(backend, source_handle);
  if (file == NULL || (source_handle.value == (uintptr_t)2u &&
                       !file_matches(file, source_id)))
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  const size_t length = backend->grow && file == &backend->files[1]
                            ? file->after_length
                            : file->after_length;
  if (backend->written_over_capacity) {
    *written = capacity == SIZE_MAX ? SIZE_MAX : capacity + 1u;
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
  }
  if (length > capacity) {
    *written = length;
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_CAPACITY;
  }
  if (length != 0u && bytes == NULL)
    return W_SEED_EPHEMERAL_PROVIDER_BACKEND_INVALID;
  if (length != 0u) (void)memcpy(bytes, file->after, length);
  (void)fill_observation(backend, file, tokens, observation, true);
  *written = length;
  return W_SEED_EPHEMERAL_PROVIDER_BACKEND_OK;
}

static void fake_close_source(void *context,
                              w_seed_ephemeral_provider_handle source_handle) {
  fake_backend *backend = context;
  if (backend != NULL) backend->close_source_invocations += 1u;
  if (backend != NULL && (source_handle.value == (uintptr_t)1u ||
                          source_handle.value == (uintptr_t)2u)) {
    record_event(backend, 'c');
    backend->close_source_calls += 1u;
    if (source_handle.value == (uintptr_t)1u)
      backend->close_source_handle1_calls += 1u;
    else
      backend->close_source_handle2_calls += 1u;
  }
}

static void fake_close_root(void *context,
                            w_seed_ephemeral_provider_handle root_handle) {
  fake_backend *backend = context;
  if (backend != NULL) backend->close_root_invocations += 1u;
  if (backend != NULL && root_handle.value == (uintptr_t)99u) {
    record_event(backend, 'D');
    backend->close_root_calls += 1u;
    backend->root_open = false;
  }
}

static w_seed_ephemeral_provider_backend fake_backend_vtable(
    fake_backend *backend) {
  const w_seed_ephemeral_provider_token_capacity provider = {8u, 8u};
  const w_seed_ephemeral_provider_token_capacity root = {4u, 4u};
  const w_seed_ephemeral_provider_token_capacity owner = {5u, 5u};
  const w_seed_ephemeral_provider_token_capacity canonical = {15u, 15u};
  return (w_seed_ephemeral_provider_backend){
      .context = backend,
      .open_root = fake_open_root,
      .open_source = fake_open_source,
      .read_source = fake_read_source,
      .revalidate_source = fake_revalidate_source,
      .close_source = fake_close_source,
      .close_root = fake_close_root,
      .metadata = {provider, root, owner, canonical}};
}

static void initialize_slot(provider_slot *slot, const char *source_id) {
  (void)memset(slot, 0, sizeof(*slot));
  (void)strncpy(slot->source_id, source_id, sizeof(slot->source_id) - 1u);
  (void)memset(slot->bytes, 0xA5, sizeof(slot->bytes));
  (void)memset(&slot->source, 0xB6, sizeof(slot->source));
  (void)memset(&slot->facts, 0xC7, sizeof(slot->facts));
  slot->provider[0] = 'x';
  slot->root_token[0] = 'x';
  slot->owner[0] = 'x';
  slot->canonical[0] = 'x';
  slot->revalidation_provider[0] = 'x';
  slot->revalidation_root_token[0] = 'x';
  slot->revalidation_owner[0] = 'x';
  slot->revalidation_canonical[0] = 'x';
  slot->request.source_id =
      (w_seed_frontend_text){slot->source_id, strlen(slot->source_id)};
  slot->request.staging_bytes = slot->staging;
  slot->request.staging_capacity = sizeof(slot->staging);
  slot->request.revalidation_bytes = slot->revalidation;
  slot->request.revalidation_capacity = sizeof(slot->revalidation);
  slot->request.bytes = slot->bytes;
  slot->request.byte_capacity = sizeof(slot->bytes);
  slot->request.source = &slot->source;
  slot->request.facts = &slot->facts;
  slot->request.tokens = (w_seed_ephemeral_provider_token_buffers){
      slot->provider, sizeof(slot->provider), slot->root_token,
      sizeof(slot->root_token), slot->owner, sizeof(slot->owner),
      slot->canonical, sizeof(slot->canonical)};
  slot->request.revalidation_tokens =
      (w_seed_ephemeral_provider_token_buffers){
          slot->revalidation_provider, sizeof(slot->revalidation_provider),
          slot->revalidation_root_token,
          sizeof(slot->revalidation_root_token), slot->revalidation_owner,
          sizeof(slot->revalidation_owner), slot->revalidation_canonical,
          sizeof(slot->revalidation_canonical)};
}

static void initialize_fixture(provider_fixture *fixture, size_t root_index) {
  static const uint8_t root_bytes[] = {'r', 'o', 'o', 't', '\n'};
  static const uint8_t child_bytes[] = {'c', 'h', 'i', 'l', 'd', '\n'};
  static const char root_path[] = "relative/root.w";
  (void)memset(fixture, 0, sizeof(*fixture));
  (void)memcpy(fixture->root_path, root_path, sizeof(root_path));
  fixture->backend.files[0] =
      (fake_file){"root.w", root_bytes, sizeof(root_bytes), root_bytes,
                  sizeof(root_bytes), "canonical-root"};
  fixture->backend.files[1] =
      (fake_file){"child.w", child_bytes, sizeof(child_bytes), child_bytes,
                  sizeof(child_bytes), "canonical-child"};
  initialize_slot(&fixture->slots[root_index], "root.w");
  initialize_slot(&fixture->slots[1u - root_index], "child.w");
  fixture->requests[0] = fixture->slots[0].request;
  fixture->requests[1] = fixture->slots[1].request;
  fixture->input.root_path =
      (w_seed_byte_view){(const uint8_t *)fixture->root_path,
                         sizeof(root_path) - 1u};
  fixture->input.requests = fixture->requests;
  fixture->input.request_count = 2u;
  fixture->input.root_request_index = root_index;
  fixture->input.limits = (w_seed_ephemeral_provider_limits){
      2u, TEST_BYTE_CAPACITY, 128u, 64u, TEST_TOKEN_CAPACITY};
  fixture->input.backend = fake_backend_vtable(&fixture->backend);
}

static void snapshot_outputs(const provider_fixture *fixture,
                             uint8_t bytes[2][TEST_BYTE_CAPACITY],
                             w_seed_source sources[2],
                             w_seed_ephemeral_graph_provider_facts facts[2]) {
  for (size_t index = 0u; index < 2u; index += 1u) {
    (void)memcpy(bytes[index], fixture->slots[index].bytes,
                 sizeof(bytes[index]));
    sources[index] = fixture->slots[index].source;
    facts[index] = fixture->slots[index].facts;
  }
}

static bool outputs_unchanged(const provider_fixture *fixture,
                              uint8_t bytes[2][TEST_BYTE_CAPACITY],
                              const w_seed_source sources[2],
                              const w_seed_ephemeral_graph_provider_facts facts[2]) {
  for (size_t index = 0u; index < 2u; index += 1u) {
    if (memcmp(bytes[index], fixture->slots[index].bytes,
               sizeof(bytes[index])) != 0 ||
        memcmp(&sources[index], &fixture->slots[index].source,
               sizeof(sources[index])) != 0 ||
        memcmp(&facts[index], &fixture->slots[index].facts,
               sizeof(facts[index])) != 0)
      return false;
  }
  return true;
}

static bool test_source_digest_helper(void) {
  static const uint8_t child_bytes[] = {'c', 'h', 'i', 'l', 'd', '\n'};
  static const uint8_t sentinel[W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES] = {
      0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u,
      0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u,
      0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u,
      0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u, 0xA5u};
  w_seed_source source;
  uint8_t digest[W_SEED_EPHEMERAL_GRAPH_SHA256_BYTES];
  (void)memcpy(digest, sentinel, sizeof(digest));
  CHECK(w_seed_source_init(
            (w_seed_byte_view){child_bytes, sizeof(child_bytes)}, &source,
            NULL));
  CHECK(!w_seed_ephemeral_graph_source_digest(NULL, digest));
  CHECK(memcmp(digest, sentinel, sizeof(digest)) == 0);
  CHECK(!w_seed_ephemeral_graph_source_digest(&source, NULL));
  CHECK(w_seed_ephemeral_graph_source_digest(&source, digest));
  CHECK(memcmp(digest, expected_child_digest, sizeof(digest)) == 0);
  return true;
}

static bool test_positive_root_not_first(void) {
  provider_fixture fixture;
  initialize_fixture(&fixture, 1u);
  CHECK(fixture.input.root_path.length == sizeof("relative/root.w") - 1u &&
        memcmp(fixture.input.root_path.data, "relative/root.w",
               sizeof("relative/root.w") - 1u) == 0);
  w_seed_ephemeral_provider_result result;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_OK);
  CHECK(result.total_source_bytes == 11u);
  CHECK(fixture.backend.open_root_calls == 1u &&
        fixture.backend.open_source_calls == 1u &&
        fixture.backend.read_calls == 2u &&
        fixture.backend.revalidation_calls == 2u);
  CHECK(fixture.backend.close_source_calls == 2u &&
        fixture.backend.close_source_invocations == 2u &&
        fixture.backend.close_source_handle1_calls == 1u &&
        fixture.backend.close_source_handle2_calls == 1u &&
        fixture.backend.close_root_calls == 1u &&
        fixture.backend.close_root_invocations == 1u);
  CHECK(fixture.backend.event_count >= 4u && fixture.backend.events[0] == 'R' &&
        fixture.backend.events[1] == 'r' && fixture.backend.events[2] == 'O' &&
        fixture.backend.events[3] == 'r');
  CHECK(fixture.slots[1].source.bytes.length == 5u &&
        memcmp(fixture.slots[1].bytes, "root\n", 5u) == 0);
  CHECK(fixture.slots[0].source.bytes.length == 6u &&
        memcmp(fixture.slots[0].bytes, "child\n", 6u) == 0);
  CHECK(fixture.slots[1].facts.snapshot_before_byte_count == 5u &&
        fixture.slots[1].facts.snapshot_after_byte_count == 5u);
  CHECK(fixture.slots[0].facts.snapshot_before_byte_count == 6u &&
        fixture.slots[0].facts.snapshot_after_byte_count == 6u);
  CHECK(memcmp(fixture.slots[0].facts.snapshot_before_digest,
               fixture.slots[0].facts.snapshot_after_digest, 32u) == 0);
  CHECK(memcmp(fixture.slots[0].facts.snapshot_before_digest,
               expected_child_digest, sizeof(expected_child_digest)) == 0 &&
        memcmp(fixture.slots[0].facts.snapshot_after_digest,
               expected_child_digest, sizeof(expected_child_digest)) == 0);
  return true;
}

static bool test_failure_outputs_unchanged(void (*configure)(fake_backend *),
                                           w_seed_ephemeral_provider_status expected,
                                           w_seed_ephemeral_provider_failure failure,
                                           size_t expected_source_closes) {
  provider_fixture fixture;
  initialize_fixture(&fixture, 0u);
  uint8_t bytes[2][TEST_BYTE_CAPACITY];
  w_seed_source sources[2];
  w_seed_ephemeral_graph_provider_facts facts[2];
  snapshot_outputs(&fixture, bytes, sources, facts);
  configure(&fixture.backend);
  w_seed_ephemeral_provider_result result;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) == expected);
  CHECK(result.failure == failure);
  CHECK(fixture.backend.close_source_calls == expected_source_closes &&
        fixture.backend.close_source_invocations == expected_source_closes &&
        fixture.backend.close_source_handle1_calls == 1u &&
        fixture.backend.close_source_handle2_calls ==
            (expected_source_closes == 2u ? 1u : 0u) &&
        fixture.backend.close_root_calls == 1u &&
        fixture.backend.close_root_invocations == 1u);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));
  return true;
}

static void configure_missing(fake_backend *backend) { backend->missing = true; }
static void configure_alias(fake_backend *backend) { backend->alias = true; }
static void configure_escape(fake_backend *backend) { backend->escape = true; }
static void configure_symlink(fake_backend *backend) {
  backend->symlink_escape = true;
}
static void configure_unproven(fake_backend *backend) {
  backend->symlink_unproven = true;
}
static void configure_mutate(fake_backend *backend) {
  static const uint8_t changed[] = {'C', 'H', 'I', 'L', 'D', '\n'};
  backend->mutate = true;
  backend->files[1].after = changed;
  backend->files[1].after_length = sizeof(changed);
}
static void configure_growth(fake_backend *backend) {
  static const uint8_t grown[] = {'c', 'h', 'i', 'l', 'd', '+', '\n'};
  backend->grow = true;
  backend->files[1].after = grown;
  backend->files[1].after_length = sizeof(grown);
}
static void configure_forged_lengths(fake_backend *backend) {
  backend->forged_lengths = true;
}
static void configure_provider_drift(fake_backend *backend) {
  backend->provider_drift = true;
}
static void configure_revalidation_drift(fake_backend *backend) {
  backend->revalidation_drift = true;
}
static void configure_token_nul(fake_backend *backend) { backend->token_nul = true; }
static void configure_written_over_capacity(fake_backend *backend) {
  backend->written_over_capacity = true;
}
static void configure_revalidation_reject(fake_backend *backend) {
  backend->reject_revalidation = true;
}

static bool test_invalid_handles(void) {
  provider_fixture fixture;
  uint8_t bytes[2][TEST_BYTE_CAPACITY];
  w_seed_source sources[2];
  w_seed_ephemeral_graph_provider_facts facts[2];
  w_seed_ephemeral_provider_result result;

  initialize_fixture(&fixture, 0u);
  fixture.backend.zero_root_handle = true;
  snapshot_outputs(&fixture, bytes, sources, facts);
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND &&
        result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT);
  CHECK(fixture.backend.close_source_invocations == 1u &&
        fixture.backend.close_root_invocations == 0u &&
        outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  fixture.backend.zero_root_source_handle = true;
  snapshot_outputs(&fixture, bytes, sources, facts);
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND &&
        result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT);
  CHECK(fixture.backend.close_source_invocations == 0u &&
        fixture.backend.close_root_invocations == 1u &&
        outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  fixture.backend.zero_root_handle = true;
  fixture.backend.zero_root_source_handle = true;
  snapshot_outputs(&fixture, bytes, sources, facts);
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND &&
        result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT);
  CHECK(fixture.backend.close_source_invocations == 0u &&
        fixture.backend.close_root_invocations == 0u &&
        outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  fixture.backend.duplicate_root_handles = true;
  snapshot_outputs(&fixture, bytes, sources, facts);
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND &&
        result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_ROOT);
  CHECK(fixture.backend.close_source_invocations == 1u &&
        fixture.backend.close_root_invocations == 0u &&
        outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  fixture.backend.duplicate_child_handle = true;
  snapshot_outputs(&fixture, bytes, sources, facts);
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND &&
        result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_OPEN_SOURCE);
  CHECK(fixture.backend.close_source_invocations == 1u &&
        fixture.backend.close_root_invocations == 1u &&
        outputs_unchanged(&fixture, bytes, sources, facts));
  return true;
}

static bool test_invalid_paths_and_ids(void) {
  provider_fixture fixture;
  initialize_fixture(&fixture, 0u);
  uint8_t bytes[2][TEST_BYTE_CAPACITY];
  w_seed_source sources[2];
  w_seed_ephemeral_graph_provider_facts facts[2];
  snapshot_outputs(&fixture, bytes, sources, facts);
  (void)memcpy(fixture.slots[1].source_id, "../escape.w", 11u);
  fixture.requests[1].source_id =
      (w_seed_frontend_text){fixture.slots[1].source_id, 11u};
  w_seed_ephemeral_provider_result result;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_PATH &&
        result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  snapshot_outputs(&fixture, bytes, sources, facts);
  fixture.root_path[0] = (char)0xC3;
  fixture.root_path[1] = (char)0x28;
  fixture.input.root_path =
      (w_seed_byte_view){(const uint8_t *)fixture.root_path, 2u};
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_INVALID_UTF8 &&
        result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  snapshot_outputs(&fixture, bytes, sources, facts);
  fixture.slots[1].source_id[0] = (char)0xC3;
  fixture.slots[1].source_id[1] = (char)0x28;
  fixture.requests[1].source_id =
      (w_seed_frontend_text){fixture.slots[1].source_id, 2u};
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_INVALID_UTF8);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  snapshot_outputs(&fixture, bytes, sources, facts);
  fixture.slots[1].source_id[0] = (char)0xC3;
  fixture.slots[1].source_id[1] = (char)0xA9;
  (void)memcpy(fixture.slots[1].source_id + 2u, ".w", 2u);
  fixture.requests[1].source_id =
      (w_seed_frontend_text){fixture.slots[1].source_id, 4u};
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_UNSUPPORTED);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_UNSUPPORTED_NFC);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  snapshot_outputs(&fixture, bytes, sources, facts);
  (void)memcpy(fixture.slots[1].source_id, "root.w", 6u);
  fixture.requests[1].source_id =
      (w_seed_frontend_text){fixture.slots[1].source_id, 6u};
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_ORDER);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));
  return true;
}

static bool test_limits_capacity_and_overlap(void) {
  provider_fixture fixture;
  initialize_fixture(&fixture, 0u);
  uint8_t bytes[2][TEST_BYTE_CAPACITY];
  w_seed_source sources[2];
  w_seed_ephemeral_graph_provider_facts facts[2];
  w_seed_ephemeral_provider_result result;
  snapshot_outputs(&fixture, bytes, sources, facts);
  fixture.slots[0].request.staging_capacity = 4u;
  fixture.requests[0] = fixture.slots[0].request;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY);
  CHECK(result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_READ &&
        result.required_byte_capacity == 5u);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  snapshot_outputs(&fixture, bytes, sources, facts);
  fixture.slots[0].request.byte_capacity = 4u;
  fixture.requests[0] = fixture.slots[0].request;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY);
  CHECK(result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_COMMIT &&
        result.required_byte_capacity == 5u);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  snapshot_outputs(&fixture, bytes, sources, facts);
  fixture.input.limits.max_source_bytes = 4u;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  snapshot_outputs(&fixture, bytes, sources, facts);
  fixture.input.limits.max_total_source_bytes = 10u;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY);
  CHECK(result.total_source_bytes == 0u || result.failure ==
                                             W_SEED_EPHEMERAL_PROVIDER_FAILURE_LIMIT);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  snapshot_outputs(&fixture, bytes, sources, facts);
  fixture.slots[0].request.revalidation_bytes = fixture.slots[0].staging;
  fixture.requests[0] = fixture.slots[0].request;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_POINTER);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  snapshot_outputs(&fixture, bytes, sources, facts);
  CHECK(w_seed_ephemeral_provider_acquire(
            &fixture.input,
            (w_seed_ephemeral_provider_result *)(void *)&fixture.input) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  snapshot_outputs(&fixture, bytes, sources, facts);
  union {
    w_seed_ephemeral_provider_result result;
    unsigned char token_storage[sizeof(w_seed_ephemeral_provider_result)];
  } aliased_token;
  (void)memset(&aliased_token, 0xD4, sizeof(aliased_token));
  unsigned char token_before[sizeof(aliased_token.token_storage)];
  (void)memcpy(token_before, aliased_token.token_storage,
               sizeof(token_before));
  fixture.slots[0].request.tokens.provider_id =
      (char *)aliased_token.token_storage;
  fixture.slots[0].request.tokens.provider_id_capacity =
      sizeof(aliased_token.token_storage);
  fixture.requests[0] = fixture.slots[0].request;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input,
                                          &aliased_token.result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(fixture.backend.open_root_calls == 0u &&
        fixture.backend.close_root_invocations == 0u &&
        memcmp(aliased_token.token_storage, token_before,
               sizeof(token_before)) == 0 &&
        outputs_unchanged(&fixture, bytes, sources, facts));
  return true;
}

static bool test_metadata_preflight_before_open(void) {
  provider_fixture fixture;
  initialize_fixture(&fixture, 0u);
  uint8_t bytes[2][TEST_BYTE_CAPACITY];
  w_seed_source sources[2];
  w_seed_ephemeral_graph_provider_facts facts[2];
  w_seed_ephemeral_provider_result result;

  snapshot_outputs(&fixture, bytes, sources, facts);
  fixture.slots[0].request.tokens.provider_id_capacity = 7u;
  fixture.requests[0] = fixture.slots[0].request;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY);
  CHECK(result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE &&
        result.request_index == 0u &&
        result.capacity_field ==
            W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_PROVIDER_ID &&
        result.required_capacity == 8u);
  CHECK(fixture.backend.open_root_calls == 0u &&
        fixture.backend.open_source_calls == 0u &&
        fixture.backend.close_source_invocations == 0u &&
        fixture.backend.close_root_invocations == 0u);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  snapshot_outputs(&fixture, bytes, sources, facts);
  fixture.slots[1].request.revalidation_tokens.canonical_token_capacity = 14u;
  fixture.requests[1] = fixture.slots[1].request;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_CAPACITY);
  CHECK(result.phase == W_SEED_EPHEMERAL_PROVIDER_PHASE_VALIDATE &&
        result.request_index == 1u &&
        result.capacity_field ==
            W_SEED_EPHEMERAL_PROVIDER_CAPACITY_FIELD_REVALIDATION_CANONICAL_TOKEN &&
        result.required_capacity == 15u);
  CHECK(fixture.backend.open_root_calls == 0u &&
        fixture.backend.open_source_calls == 0u &&
        fixture.backend.close_source_invocations == 0u &&
        fixture.backend.close_root_invocations == 0u);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));
  return true;
}

static bool test_provider_faults(void) {
  CHECK(test_failure_outputs_unchanged(
            configure_missing, W_SEED_EPHEMERAL_PROVIDER_IO,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_SOURCE_MISSING, 1u));
  CHECK(test_failure_outputs_unchanged(
            configure_alias, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_CANONICAL_ALIAS, 2u));
  CHECK(test_failure_outputs_unchanged(
            configure_escape, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_CONTAINMENT, 1u));
  CHECK(test_failure_outputs_unchanged(
            configure_symlink, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK, 2u));
  CHECK(test_failure_outputs_unchanged(
            configure_unproven, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_SYMLINK, 2u));
  CHECK(test_failure_outputs_unchanged(
            configure_mutate, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_SNAPSHOT, 2u));
  CHECK(test_failure_outputs_unchanged(
            configure_growth, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_SNAPSHOT, 2u));
  CHECK(test_failure_outputs_unchanged(
            configure_forged_lengths, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_TOKEN, 1u));
  CHECK(test_failure_outputs_unchanged(
            configure_provider_drift, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_TOKEN, 2u));
  CHECK(test_failure_outputs_unchanged(
            configure_revalidation_drift, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_SNAPSHOT, 2u));
  CHECK(test_failure_outputs_unchanged(
            configure_token_nul, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_TOKEN, 1u));
  CHECK(test_failure_outputs_unchanged(
            configure_written_over_capacity, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND, 2u));
  CHECK(test_failure_outputs_unchanged(
            configure_revalidation_reject, W_SEED_EPHEMERAL_PROVIDER_INVALID,
            W_SEED_EPHEMERAL_PROVIDER_FAILURE_BACKEND, 2u));
  return true;
}

static bool test_invalid_source_and_zero_source(void) {
  provider_fixture fixture;
  initialize_fixture(&fixture, 0u);
  static const uint8_t invalid[] = {0xC3u};
  fixture.backend.files[0].before = invalid;
  fixture.backend.files[0].before_length = sizeof(invalid);
  fixture.backend.files[0].after = invalid;
  fixture.backend.files[0].after_length = sizeof(invalid);
  uint8_t bytes[2][TEST_BYTE_CAPACITY];
  w_seed_source sources[2];
  w_seed_ephemeral_graph_provider_facts facts[2];
  snapshot_outputs(&fixture, bytes, sources, facts);
  w_seed_ephemeral_provider_result result;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_INVALID);
  CHECK(result.failure == W_SEED_EPHEMERAL_PROVIDER_FAILURE_SOURCE_ENCODING);
  CHECK(outputs_unchanged(&fixture, bytes, sources, facts));

  initialize_fixture(&fixture, 0u);
  fixture.backend.files[0].before = NULL;
  fixture.backend.files[0].before_length = 0u;
  fixture.backend.files[0].after = NULL;
  fixture.backend.files[0].after_length = 0u;
  fixture.slots[0].request.staging_bytes = NULL;
  fixture.slots[0].request.staging_capacity = 0u;
  fixture.slots[0].request.revalidation_bytes = NULL;
  fixture.slots[0].request.revalidation_capacity = 0u;
  fixture.slots[0].request.bytes = NULL;
  fixture.slots[0].request.byte_capacity = 0u;
  fixture.requests[0] = fixture.slots[0].request;
  CHECK(w_seed_ephemeral_provider_acquire(&fixture.input, &result) ==
        W_SEED_EPHEMERAL_PROVIDER_OK);
  CHECK(fixture.slots[0].source.bytes.data == NULL &&
        fixture.slots[0].source.bytes.length == 0u);
  return true;
}

int main(void) {
  if (!test_source_digest_helper()) return 1;
  if (!test_positive_root_not_first()) return 1;
  if (!test_invalid_paths_and_ids()) return 1;
  if (!test_limits_capacity_and_overlap()) return 1;
  if (!test_metadata_preflight_before_open()) return 1;
  if (!test_provider_faults()) return 1;
  if (!test_invalid_handles()) return 1;
  if (!test_invalid_source_and_zero_source()) return 1;
  (void)printf("RESULT provider-core=pass\n");
  return 0;
}
