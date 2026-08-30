#include "w_cli_io.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "cli io check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                      \
      return false;                                                            \
    }                                                                          \
  } while (0)

typedef struct {
  FILE *expected_stream;
  bool wrong_stream;
  int set_binary_result;
  int put_text_result;
  size_t write_result;
  int flush_result;
  size_t set_binary_calls;
  size_t put_text_calls;
  size_t write_calls;
  size_t flush_calls;
  const char *text;
  const void *bytes;
  size_t size;
  size_t count;
} fake_state;

static fake_state *fake_from_context(void *context, FILE *stream) {
  fake_state *fake = (fake_state *)context;
  if (fake != NULL && stream != fake->expected_stream) fake->wrong_stream = true;
  return fake;
}

static int fake_set_binary(void *context, FILE *stream) {
  fake_state *fake = fake_from_context(context, stream);
  fake->set_binary_calls += 1u;
  return fake->set_binary_result;
}

static int fake_put_text(void *context, const char *text, FILE *stream) {
  fake_state *fake = fake_from_context(context, stream);
  fake->put_text_calls += 1u;
  fake->text = text;
  return fake->put_text_result;
}

static size_t fake_write(void *context, const void *bytes, size_t size,
                         size_t count, FILE *stream) {
  fake_state *fake = fake_from_context(context, stream);
  fake->write_calls += 1u;
  fake->bytes = bytes;
  fake->size = size;
  fake->count = count;
  return fake->write_result;
}

static int fake_flush(void *context, FILE *stream) {
  fake_state *fake = fake_from_context(context, stream);
  fake->flush_calls += 1u;
  return fake->flush_result;
}

static w_seed_cli_io_ops fake_ops(fake_state *fake) {
  return (w_seed_cli_io_ops){fake, fake_set_binary, fake_put_text, fake_write,
                             fake_flush};
}

static bool test_binary_mode(void) {
  FILE *stream = tmpfile();
  CHECK(stream != NULL);
  fake_state fake = {.expected_stream = stream};
  w_seed_cli_io_ops ops = fake_ops(&fake);
  CHECK(w_seed_cli_prepare_binary(stream, &ops));
  CHECK(fake.set_binary_calls == 1u);
  fake.set_binary_result = -1;
  CHECK(!w_seed_cli_prepare_binary(stream, &ops));
  CHECK(fake.set_binary_calls == 2u);
  CHECK(!fake.wrong_stream);
  CHECK(!w_seed_cli_prepare_binary(NULL, &ops));
  CHECK(!w_seed_cli_prepare_binary(stream, NULL));
  CHECK(fclose(stream) == 0);
  return true;
}

static bool test_text_write_and_flush(void) {
  FILE *stream = tmpfile();
  CHECK(stream != NULL);
  fake_state fake = {.expected_stream = stream};
  w_seed_cli_io_ops ops = fake_ops(&fake);
  CHECK(w_seed_cli_write_text(stream, "usage\n", &ops));
  CHECK(fake.put_text_calls == 1u && fake.flush_calls == 1u);
  CHECK(strcmp(fake.text, "usage\n") == 0);

  fake = (fake_state){.expected_stream = stream, .put_text_result = -1};
  ops = fake_ops(&fake);
  CHECK(!w_seed_cli_write_text(stream, "usage\n", &ops));
  CHECK(fake.put_text_calls == 1u && fake.flush_calls == 1u);

  fake = (fake_state){.expected_stream = stream, .flush_result = -1};
  ops = fake_ops(&fake);
  CHECK(!w_seed_cli_write_text(stream, "usage\n", &ops));
  CHECK(fake.put_text_calls == 1u && fake.flush_calls == 1u);
  CHECK(!fake.wrong_stream);
  CHECK(fclose(stream) == 0);
  return true;
}

static bool test_byte_write_reports_effects(void) {
  static const uint8_t output[] = "Hello, world!\n";
  FILE *stream = tmpfile();
  CHECK(stream != NULL);
  fake_state fake = {.expected_stream = stream,
                     .write_result = sizeof(output) - 1u};
  w_seed_cli_io_ops ops = fake_ops(&fake);
  w_seed_cli_write_result result = w_seed_cli_write_bytes_with_ops(
      stream, output, sizeof(output) - 1u, &ops);
  CHECK(result.accepted_bytes == sizeof(output) - 1u);
  CHECK(result.flush_status == W_SEED_CLI_FLUSH_SUCCEEDED);
  CHECK(fake.write_calls == 1u && fake.flush_calls == 1u);
  CHECK(fake.bytes == output && fake.size == 1u &&
        fake.count == sizeof(output) - 1u);

  fake = (fake_state){.expected_stream = stream, .write_result = 5u};
  ops = fake_ops(&fake);
  result = w_seed_cli_write_bytes_with_ops(stream, output,
                                           sizeof(output) - 1u, &ops);
  CHECK(result.accepted_bytes == 5u);
  CHECK(result.flush_status == W_SEED_CLI_FLUSH_NOT_ATTEMPTED);
  CHECK(fake.write_calls == 1u && fake.flush_calls == 0u);

  fake = (fake_state){.expected_stream = stream,
                      .write_result = sizeof(output) - 1u,
                      .flush_result = -1};
  ops = fake_ops(&fake);
  result = w_seed_cli_write_bytes_with_ops(stream, output,
                                           sizeof(output) - 1u, &ops);
  CHECK(result.accepted_bytes == sizeof(output) - 1u);
  CHECK(result.flush_status == W_SEED_CLI_FLUSH_FAILED);
  CHECK(fake.write_calls == 1u && fake.flush_calls == 1u);

  fake = (fake_state){.expected_stream = stream};
  ops = fake_ops(&fake);
  result = w_seed_cli_write_bytes_with_ops(stream, output,
                                           sizeof(output) - 1u, &ops);
  CHECK(result.accepted_bytes == 0u);
  CHECK(result.flush_status == W_SEED_CLI_FLUSH_NOT_ATTEMPTED);
  CHECK(fake.write_calls == 1u && fake.flush_calls == 0u);

  fake = (fake_state){.expected_stream = stream,
                      .write_result = sizeof(output) - 1u};
  ops = fake_ops(&fake);
  CHECK(w_seed_cli_write_bytes(stream, output, sizeof(output) - 1u, &ops));
  fake = (fake_state){.expected_stream = stream, .write_result = 1u};
  ops = fake_ops(&fake);
  CHECK(!w_seed_cli_write_bytes(stream, output, sizeof(output) - 1u, &ops));

  fake = (fake_state){.expected_stream = stream};
  ops = fake_ops(&fake);
  CHECK(w_seed_cli_flush(stream, &ops));
  fake.flush_result = -1;
  CHECK(!w_seed_cli_flush(stream, &ops));
  CHECK(!fake.wrong_stream);
  CHECK(fclose(stream) == 0);
  return true;
}

int main(void) {
  if (!test_binary_mode() || !test_text_write_and_flush() ||
      !test_byte_write_reports_effects())
    return 1;
  (void)puts("cli io unit: binary mode, text flush, and byte reports passed");
  return 0;
}
