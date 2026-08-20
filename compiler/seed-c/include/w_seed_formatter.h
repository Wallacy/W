#ifndef W_SEED_FORMATTER_H
#define W_SEED_FORMATTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Formatter storage is caller-owned. The formatter never allocates and never
 * truncates a result. Token and delimiter capacity should be at least the
 * parser node capacity for a lossless CST. */
typedef struct {
  w_seed_span span;
  uint8_t kind;
  uint8_t flags;
} w_seed_format_token;

typedef struct {
  size_t open_token;
  size_t close_token;
  char open_char;
  char close_char;
  w_seed_cst_index owner;
  bool multiline;
} w_seed_format_group;

typedef struct {
  w_seed_format_token *tokens;
  size_t token_capacity;
  w_seed_format_group *groups;
  size_t group_capacity;
} w_seed_formatter_buffers;

typedef enum {
  W_SEED_FORMAT_OK = 0,
  W_SEED_FORMAT_REJECTED,
  W_SEED_FORMAT_CAPACITY,
  W_SEED_FORMAT_INVALID,
} w_seed_format_status;

typedef struct {
  w_seed_format_status status;
  size_t required_bytes;
  size_t written_bytes;
  size_t token_count;
  size_t group_count;
} w_seed_format_result;

/* Format one clean COMPLETE parser CST. A rejected/recovered/fatal CST is
 * never rendered. Pass output=NULL, capacity=0 to measure; this returns
 * W_SEED_FORMAT_CAPACITY with required_bytes and no output writes. */
w_seed_format_status w_seed_formatter_format(
    const w_seed_parser *parser, const w_seed_formatter_buffers *buffers,
    uint8_t *output, size_t output_capacity, w_seed_format_result *result);

#ifdef __cplusplus
}
#endif

#endif
