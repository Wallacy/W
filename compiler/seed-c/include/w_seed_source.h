#ifndef W_SEED_SOURCE_H
#define W_SEED_SOURCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Internal seed-C source views. These types do not own or copy byte storage. */
typedef struct {
  const uint8_t *data;
  size_t length;
} w_seed_byte_view;

typedef struct {
  size_t start_byte;
  size_t end_byte;
} w_seed_span;

typedef struct {
  size_t line;
  size_t byte_column;
} w_seed_source_point;

typedef enum {
  W_SEED_SOURCE_ERROR_NONE = 0,
  W_SEED_SOURCE_ERROR_NULL_ARGUMENT,
  W_SEED_SOURCE_ERROR_SOURCE_TOO_LARGE,
  W_SEED_SOURCE_ERROR_INVALID_UTF8_LEAD,
  W_SEED_SOURCE_ERROR_UTF8_STRAY_CONTINUATION,
  W_SEED_SOURCE_ERROR_UTF8_TRUNCATED,
  W_SEED_SOURCE_ERROR_UTF8_BAD_CONTINUATION,
  W_SEED_SOURCE_ERROR_UTF8_OVERLONG,
  W_SEED_SOURCE_ERROR_UTF8_SURROGATE,
  W_SEED_SOURCE_ERROR_UTF8_OUT_OF_RANGE,
  W_SEED_SOURCE_ERROR_SPAN_ORDER,
  W_SEED_SOURCE_ERROR_SPAN_OUT_OF_RANGE,
  W_SEED_SOURCE_ERROR_NOT_CODE_POINT_BOUNDARY,
  W_SEED_SOURCE_ERROR_OFFSET_OUT_OF_RANGE,
  W_SEED_SOURCE_ERROR_LINE_OUT_OF_RANGE,
  W_SEED_SOURCE_ERROR_COLUMN_OUT_OF_RANGE,
} w_seed_source_error_kind;

typedef struct {
  w_seed_source_error_kind kind;
  size_t byte_offset;
} w_seed_source_error;

typedef struct {
  w_seed_byte_view bytes;
  size_t line_count;
  size_t bom_length;
} w_seed_source;

/* Initialize a source view and validate every UTF-8 sequence. */
bool w_seed_source_init(w_seed_byte_view bytes, w_seed_source *source,
                        w_seed_source_error *error);

/* Return the original bytes. The result aliases the caller-owned view. */
w_seed_byte_view w_seed_source_bytes(const w_seed_source *source);

/* Return true when the source starts with the UTF-8 BOM. The BOM remains in bytes. */
bool w_seed_source_has_bom(const w_seed_source *source);

/* Return the number of lines. An empty source has one line. */
size_t w_seed_source_line_count(const w_seed_source *source);

/* Validate a half-open byte span and both code-point boundaries. */
bool w_seed_source_validate_span(const w_seed_source *source, w_seed_span span,
                                 w_seed_source_error *error);

/* Return a zero-copy slice after validating its span. Empty slices have data=NULL. */
bool w_seed_source_slice(const w_seed_source *source, w_seed_span span,
                         w_seed_byte_view *slice, w_seed_source_error *error);

/* Convert a code-point-boundary byte offset to a zero-based line and byte column. */
bool w_seed_source_offset_to_point(const w_seed_source *source, size_t offset,
                                   w_seed_source_point *point,
                                   w_seed_source_error *error);

/* Convert a zero-based line and byte column to a code-point-boundary byte offset. */
bool w_seed_source_point_to_offset(const w_seed_source *source,
                                   w_seed_source_point point, size_t *offset,
                                   w_seed_source_error *error);

#ifdef __cplusplus
}
#endif

#endif
