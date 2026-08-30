#ifndef W_SEED_CLI_IO_H
#define W_SEED_CLI_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void *context;
  int (*set_binary)(void *context, FILE *stream);
  int (*put_text)(void *context, const char *text, FILE *stream);
  size_t (*write_bytes)(void *context, const void *bytes, size_t size,
                        size_t count, FILE *stream);
  int (*flush)(void *context, FILE *stream);
} w_seed_cli_io_ops;

extern const w_seed_cli_io_ops w_seed_cli_stdio_ops;

typedef enum {
  W_SEED_CLI_FLUSH_NOT_ATTEMPTED = 0,
  W_SEED_CLI_FLUSH_SUCCEEDED,
  W_SEED_CLI_FLUSH_FAILED,
} w_seed_cli_flush_status;

typedef struct {
  size_t accepted_bytes;
  w_seed_cli_flush_status flush_status;
} w_seed_cli_write_result;

bool w_seed_cli_prepare_binary(FILE *stream, const w_seed_cli_io_ops *ops);

bool w_seed_cli_write_text(FILE *stream, const char *text,
                           const w_seed_cli_io_ops *ops);

w_seed_cli_write_result w_seed_cli_write_bytes_with_ops(
    FILE *stream, const uint8_t *bytes, size_t byte_count,
    const w_seed_cli_io_ops *ops);

bool w_seed_cli_write_bytes(FILE *stream, const uint8_t *bytes,
                            size_t byte_count,
                            const w_seed_cli_io_ops *ops);

bool w_seed_cli_flush(FILE *stream, const w_seed_cli_io_ops *ops);

#ifdef __cplusplus
}
#endif

#endif
