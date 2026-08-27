#include "check_host.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "check host test failed: %s (%s:%d)\n",        \
                    #condition, __FILE__, __LINE__);                          \
      return false;                                                            \
    }                                                                          \
  } while (0)

typedef struct {
  const char *path;
  size_t length;
  w_seed_check_host_status status;
  const char *source_id;
  size_t source_id_length;
} path_case;

static bool test_root_source_id_table(void) {
#if defined(_WIN32)
  static const path_case cases[] = {
      {"root.w", sizeof("root.w") - 1u, W_SEED_CHECK_HOST_OK, "root.w", 6u},
      {"dir/root.w", sizeof("dir/root.w") - 1u, W_SEED_CHECK_HOST_OK,
       "root.w", 6u},
      {"dir\\root.w", sizeof("dir\\root.w") - 1u, W_SEED_CHECK_HOST_OK,
       "root.w", 6u},
      {"\xC3\xA1rea/root.w", sizeof("\xC3\xA1rea/root.w") - 1u,
       W_SEED_CHECK_HOST_OK, "root.w", 6u},
      {"dir/ro\xC3\xB3t.w", sizeof("dir/ro\xC3\xB3t.w") - 1u,
       W_SEED_CHECK_HOST_UNSUPPORTED, NULL, 0u},
      {"dir/a-b.w", sizeof("dir/a-b.w") - 1u, W_SEED_CHECK_HOST_INVALID,
       NULL, 0u},
      {"dir/.w", sizeof("dir/.w") - 1u, W_SEED_CHECK_HOST_INVALID, NULL, 0u},
      {"dir/a.b.w", sizeof("dir/a.b.w") - 1u, W_SEED_CHECK_HOST_INVALID,
       NULL, 0u},
      {"dir/root", sizeof("dir/root") - 1u, W_SEED_CHECK_HOST_INVALID, NULL,
       0u},
      {"dir/", sizeof("dir/") - 1u, W_SEED_CHECK_HOST_INVALID, NULL, 0u},
      {"/", sizeof("/") - 1u, W_SEED_CHECK_HOST_INVALID, NULL, 0u},
      {"dir/_r9.w", sizeof("dir/_r9.w") - 1u, W_SEED_CHECK_HOST_OK,
       "_r9.w", 5u},
  };
#else
  static const path_case cases[] = {
      {"root.w", sizeof("root.w") - 1u, W_SEED_CHECK_HOST_OK, "root.w", 6u},
      {"dir/root.w", sizeof("dir/root.w") - 1u, W_SEED_CHECK_HOST_OK,
       "root.w", 6u},
      {"dir\\root.w", sizeof("dir\\root.w") - 1u,
       W_SEED_CHECK_HOST_INVALID, NULL, 0u},
      {"\xC3\xA1rea/root.w", sizeof("\xC3\xA1rea/root.w") - 1u,
       W_SEED_CHECK_HOST_OK, "root.w", 6u},
      {"dir/ro\xC3\xB3t.w", sizeof("dir/ro\xC3\xB3t.w") - 1u,
       W_SEED_CHECK_HOST_UNSUPPORTED, NULL, 0u},
      {"dir/a-b.w", sizeof("dir/a-b.w") - 1u, W_SEED_CHECK_HOST_INVALID,
       NULL, 0u},
      {"dir/.w", sizeof("dir/.w") - 1u, W_SEED_CHECK_HOST_INVALID, NULL, 0u},
      {"dir/a.b.w", sizeof("dir/a.b.w") - 1u, W_SEED_CHECK_HOST_INVALID,
       NULL, 0u},
      {"dir/root", sizeof("dir/root") - 1u, W_SEED_CHECK_HOST_INVALID, NULL,
       0u},
      {"dir/", sizeof("dir/") - 1u, W_SEED_CHECK_HOST_INVALID, NULL, 0u},
      {"/", sizeof("/") - 1u, W_SEED_CHECK_HOST_INVALID, NULL, 0u},
      {"dir/_r9.w", sizeof("dir/_r9.w") - 1u, W_SEED_CHECK_HOST_OK,
       "_r9.w", 5u},
  };
#endif
  static const char embedded_nul[] = {'d', '/', 'r', 'o', 'o', 't', '\0',
                                      '.', 'w'};
  w_seed_frontend_text source_id;
  for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]);
       index += 1u) {
    source_id = (w_seed_frontend_text){(const char *)"sentinel", 8u};
    const w_seed_check_host_status status = w_seed_check_root_source_id(
        cases[index].path, cases[index].length, &source_id);
    CHECK(status == cases[index].status);
    if (status == W_SEED_CHECK_HOST_OK) {
      CHECK(source_id.data != NULL &&
            source_id.length == cases[index].source_id_length);
      CHECK(memcmp(source_id.data, cases[index].source_id,
                   source_id.length) == 0);
      CHECK(source_id.data ==
            cases[index].path + cases[index].length - source_id.length);
    } else {
      CHECK(source_id.data == NULL && source_id.length == 0u);
    }
  }
  source_id = (w_seed_frontend_text){(const char *)"sentinel", 8u};
  CHECK(w_seed_check_root_source_id(embedded_nul, sizeof(embedded_nul),
                                    &source_id) == W_SEED_CHECK_HOST_INVALID);
  CHECK(source_id.data == NULL && source_id.length == 0u);
  CHECK(w_seed_check_root_source_id(NULL, 0u, &source_id) ==
        W_SEED_CHECK_HOST_INVALID);
  CHECK(w_seed_check_root_source_id("root.w", 6u, NULL) ==
        W_SEED_CHECK_HOST_INVALID);
  return true;
}

static bool backend_shape(const w_seed_ephemeral_provider_backend *backend) {
  return backend != NULL && backend->context != NULL &&
         backend->open_root != NULL && backend->open_source != NULL &&
         backend->read_source != NULL && backend->revalidate_source != NULL &&
         backend->close_source != NULL && backend->close_root != NULL &&
         backend->metadata.provider_id.required_capacity != 0u &&
         backend->metadata.root_token.required_capacity != 0u;
}

static bool test_host_open_close_lifecycle(void) {
  w_seed_check_host host = {0};
  w_seed_ephemeral_provider_backend backend = {0};
  CHECK(w_seed_check_host_init(&host) == W_SEED_CHECK_HOST_OK);
  const w_seed_check_host initialized_snapshot = host;
  CHECK(w_seed_check_host_init(&host) == W_SEED_CHECK_HOST_INVALID);
  CHECK(host.initialized && !host.open &&
        memcmp(&host.backend, &initialized_snapshot.backend,
               sizeof(host.backend)) == 0);
  const w_seed_check_host_status status =
      w_seed_check_host_open(&host, &backend);
#if defined(__linux__) || defined(_WIN32)
  CHECK(status == W_SEED_CHECK_HOST_OK ||
        status == W_SEED_CHECK_HOST_UNSUPPORTED);
#else
  CHECK(status == W_SEED_CHECK_HOST_UNSUPPORTED);
#endif
  if (status == W_SEED_CHECK_HOST_OK) {
    CHECK(host.open);
    CHECK(backend_shape(&backend));
    CHECK(host.backend.context == backend.context);
    w_seed_ephemeral_provider_backend second_backend;
    (void)memset(&second_backend, 0xA5, sizeof(second_backend));
    CHECK(w_seed_check_host_open(&host, &second_backend) ==
          W_SEED_CHECK_HOST_INVALID);
    const uint8_t *second_bytes = (const uint8_t *)&second_backend;
    for (size_t index = 0u; index < sizeof(second_backend); index += 1u)
      CHECK(second_bytes[index] == 0xA5u);
  }
  w_seed_check_host_close(&host);
  CHECK(!host.open);
  w_seed_check_host_close(&host);
  CHECK(!host.open);
  (void)memset(&backend, 0, sizeof(backend));
  CHECK(w_seed_check_host_open(&host, &backend) == status);
  w_seed_check_host_close(&host);
  return true;
}

int main(void) {
  if (!test_root_source_id_table() || !test_host_open_close_lifecycle())
    return 1;
  (void)puts("w_seed_check_host_tests: ok");
  return 0;
}
