#include "w_cli_io.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int stdio_set_binary(void *context, FILE *stream) {
  (void)context;
  if (stream == NULL) return -1;
#ifdef _WIN32
  const int descriptor = _fileno(stream);
  if (descriptor == -1 || _setmode(descriptor, _O_BINARY) == -1) return -1;
#endif
  return 0;
}

static int stdio_put_text(void *context, const char *text, FILE *stream) {
  (void)context;
  return fputs(text, stream);
}

static size_t stdio_write_bytes(void *context, const void *bytes, size_t size,
                                size_t count, FILE *stream) {
  (void)context;
  return fwrite(bytes, size, count, stream);
}

static int stdio_flush(void *context, FILE *stream) {
  (void)context;
  return fflush(stream);
}

const w_seed_cli_io_ops w_seed_cli_stdio_ops = {
    NULL,
    stdio_set_binary,
    stdio_put_text,
    stdio_write_bytes,
    stdio_flush,
};

static bool valid_ops(const w_seed_cli_io_ops *ops) {
  return ops != NULL && ops->set_binary != NULL && ops->put_text != NULL &&
         ops->write_bytes != NULL && ops->flush != NULL;
}

bool w_seed_cli_prepare_binary(FILE *stream, const w_seed_cli_io_ops *ops) {
  return stream != NULL && valid_ops(ops) &&
         ops->set_binary(ops->context, stream) == 0;
}

bool w_seed_cli_write_text(FILE *stream, const char *text,
                           const w_seed_cli_io_ops *ops) {
  if (stream == NULL || text == NULL || !valid_ops(ops)) return false;
  const bool put_ok = ops->put_text(ops->context, text, stream) >= 0;
  const bool flush_ok = ops->flush(ops->context, stream) == 0;
  return put_ok && flush_ok;
}

w_seed_cli_write_result w_seed_cli_write_bytes_with_ops(
    FILE *stream, const uint8_t *bytes, size_t byte_count,
    const w_seed_cli_io_ops *ops) {
  if (stream == NULL || bytes == NULL || !valid_ops(ops)) {
    return (w_seed_cli_write_result){0u,
                                     W_SEED_CLI_FLUSH_NOT_ATTEMPTED};
  }
  const size_t accepted =
      ops->write_bytes(ops->context, bytes, 1u, byte_count, stream);
  if (accepted != byte_count) {
    return (w_seed_cli_write_result){accepted,
                                     W_SEED_CLI_FLUSH_NOT_ATTEMPTED};
  }
  return (w_seed_cli_write_result){
      accepted, ops->flush(ops->context, stream) == 0
                    ? W_SEED_CLI_FLUSH_SUCCEEDED
                    : W_SEED_CLI_FLUSH_FAILED};
}

bool w_seed_cli_write_bytes(FILE *stream, const uint8_t *bytes,
                            size_t byte_count,
                            const w_seed_cli_io_ops *ops) {
  const w_seed_cli_write_result result =
      w_seed_cli_write_bytes_with_ops(stream, bytes, byte_count, ops);
  return result.accepted_bytes == byte_count &&
         result.flush_status == W_SEED_CLI_FLUSH_SUCCEEDED;
}

bool w_seed_cli_flush(FILE *stream, const w_seed_cli_io_ops *ops) {
  return stream != NULL && valid_ops(ops) &&
         ops->flush(ops->context, stream) == 0;
}
