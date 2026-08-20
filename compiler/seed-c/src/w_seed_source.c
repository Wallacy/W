#include "w_seed_source.h"

#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "w_seed_source requires 8-bit bytes");

static void clear_error(w_seed_source_error *error) {
  if (error == NULL) return;
  error->kind = W_SEED_SOURCE_ERROR_NONE;
  error->byte_offset = 0;
}

static bool fail(w_seed_source_error *error, w_seed_source_error_kind kind,
                 size_t byte_offset) {
  if (error != NULL) {
    error->kind = kind;
    error->byte_offset = byte_offset;
  }
  return false;
}

static bool is_continuation(uint8_t byte) {
  return byte >= 0x80u && byte <= 0xBFu;
}

static bool valid_source_argument(const w_seed_byte_view bytes,
                                  w_seed_source_error *error) {
  if (bytes.length != 0 && bytes.data == NULL) {
    return fail(error, W_SEED_SOURCE_ERROR_NULL_ARGUMENT, 0);
  }
  return true;
}

static bool validate_utf8(w_seed_byte_view bytes, w_seed_source_error *error) {
  size_t index = 0;
  while (index < bytes.length) {
    const uint8_t first = bytes.data[index];
    if (first <= 0x7Fu) {
      index += 1;
      continue;
    }

    if (first >= 0x80u && first <= 0xBFu) {
      return fail(error, W_SEED_SOURCE_ERROR_UTF8_STRAY_CONTINUATION, index);
    }
    if (first == 0xC0u || first == 0xC1u) {
      return fail(error, W_SEED_SOURCE_ERROR_UTF8_OVERLONG, index);
    }
    if (first >= 0xF5u && first <= 0xF7u) {
      return fail(error, W_SEED_SOURCE_ERROR_UTF8_OUT_OF_RANGE, index);
    }
    if (first >= 0xF8u) {
      return fail(error, W_SEED_SOURCE_ERROR_INVALID_UTF8_LEAD, index);
    }

    size_t width = 0;
    if (first >= 0xC2u && first <= 0xDFu) {
      width = 2;
    } else if (first >= 0xE0u && first <= 0xEFu) {
      width = 3;
    } else if (first >= 0xF0u && first <= 0xF4u) {
      width = 4;
    } else {
      return fail(error, W_SEED_SOURCE_ERROR_INVALID_UTF8_LEAD, index);
    }

    if (bytes.length - index < width) {
      return fail(error, W_SEED_SOURCE_ERROR_UTF8_TRUNCATED, index);
    }

    const uint8_t second = bytes.data[index + 1];
    if (!is_continuation(second)) {
      return fail(error, W_SEED_SOURCE_ERROR_UTF8_BAD_CONTINUATION, index + 1);
    }
    if (width == 3 && first == 0xE0u && second < 0xA0u) {
      return fail(error, W_SEED_SOURCE_ERROR_UTF8_OVERLONG, index + 1);
    }
    if (width == 3 && first == 0xEDu && second >= 0xA0u) {
      return fail(error, W_SEED_SOURCE_ERROR_UTF8_SURROGATE, index + 1);
    }
    if (width == 4 && first == 0xF0u && second < 0x90u) {
      return fail(error, W_SEED_SOURCE_ERROR_UTF8_OVERLONG, index + 1);
    }
    if (width == 4 && first == 0xF4u && second > 0x8Fu) {
      return fail(error, W_SEED_SOURCE_ERROR_UTF8_OUT_OF_RANGE, index + 1);
    }

    for (size_t continuation = 2; continuation < width; continuation += 1) {
      if (!is_continuation(bytes.data[index + continuation])) {
        return fail(error, W_SEED_SOURCE_ERROR_UTF8_BAD_CONTINUATION,
                    index + continuation);
      }
    }
    index += width;
  }
  return true;
}

static bool count_lines(w_seed_byte_view bytes, size_t *lines,
                        w_seed_source_error *error) {
  if (lines == NULL) {
    return fail(error, W_SEED_SOURCE_ERROR_NULL_ARGUMENT, 0);
  }
  *lines = 1;
  for (size_t index = 0; index < bytes.length; index += 1) {
    if (bytes.data[index] == 0x0Au) {
      if (*lines == SIZE_MAX) {
        return fail(error, W_SEED_SOURCE_ERROR_SOURCE_TOO_LARGE, index);
      }
      *lines += 1;
    }
  }
  return true;
}

static bool is_code_point_boundary(const w_seed_source *source, size_t offset) {
  if (offset > source->bytes.length) return false;
  if (offset == 0 || offset == source->bytes.length) return true;
  return !is_continuation(source->bytes.data[offset]);
}

static bool source_ready(const w_seed_source *source,
                         w_seed_source_error *error) {
  if (source == NULL) {
    return fail(error, W_SEED_SOURCE_ERROR_NULL_ARGUMENT, 0);
  }
  if (source->bytes.length != 0 && source->bytes.data == NULL) {
    return fail(error, W_SEED_SOURCE_ERROR_NULL_ARGUMENT, 0);
  }
  return true;
}

static size_t line_start(const w_seed_source *source, size_t line) {
  size_t current = 0;
  size_t start = 0;
  for (size_t index = 0; index < source->bytes.length && current < line;
       index += 1) {
    if (source->bytes.data[index] == 0x0Au) {
      current += 1;
      start = index + 1;
    }
  }
  return start;
}

static size_t line_end(const w_seed_source *source, size_t start) {
  for (size_t index = start; index < source->bytes.length; index += 1) {
    if (source->bytes.data[index] == 0x0Au) return index;
  }
  return source->bytes.length;
}

bool w_seed_source_init(w_seed_byte_view bytes, w_seed_source *source,
                        w_seed_source_error *error) {
  clear_error(error);
  if (source == NULL) {
    return fail(error, W_SEED_SOURCE_ERROR_NULL_ARGUMENT, 0);
  }
  source->bytes.data = NULL;
  source->bytes.length = 0;
  source->line_count = 0;
  source->bom_length = 0;
  if (!valid_source_argument(bytes, error)) return false;
  if (!validate_utf8(bytes, error)) return false;

  source->bytes = bytes;
  if (!count_lines(bytes, &source->line_count, error)) {
    source->bytes.data = NULL;
    source->bytes.length = 0;
    source->line_count = 0;
    return false;
  }
  if (bytes.length >= 3 && bytes.data[0] == 0xEFu &&
      bytes.data[1] == 0xBBu && bytes.data[2] == 0xBFu) {
    source->bom_length = 3;
  }
  return true;
}

w_seed_byte_view w_seed_source_bytes(const w_seed_source *source) {
  if (source == NULL) {
    const w_seed_byte_view empty = {NULL, 0};
    return empty;
  }
  return source->bytes;
}

bool w_seed_source_has_bom(const w_seed_source *source) {
  return source != NULL && source->bom_length == 3;
}

size_t w_seed_source_line_count(const w_seed_source *source) {
  return source == NULL ? 0 : source->line_count;
}

bool w_seed_source_validate_span(const w_seed_source *source, w_seed_span span,
                                 w_seed_source_error *error) {
  clear_error(error);
  if (!source_ready(source, error)) return false;
  if (span.start_byte > span.end_byte) {
    return fail(error, W_SEED_SOURCE_ERROR_SPAN_ORDER, span.start_byte);
  }
  if (span.end_byte > source->bytes.length) {
    return fail(error, W_SEED_SOURCE_ERROR_SPAN_OUT_OF_RANGE, span.end_byte);
  }
  if (!is_code_point_boundary(source, span.start_byte)) {
    return fail(error, W_SEED_SOURCE_ERROR_NOT_CODE_POINT_BOUNDARY,
                span.start_byte);
  }
  if (!is_code_point_boundary(source, span.end_byte)) {
    return fail(error, W_SEED_SOURCE_ERROR_NOT_CODE_POINT_BOUNDARY,
                span.end_byte);
  }
  return true;
}

bool w_seed_source_slice(const w_seed_source *source, w_seed_span span,
                         w_seed_byte_view *slice,
                         w_seed_source_error *error) {
  clear_error(error);
  if (slice == NULL) {
    return fail(error, W_SEED_SOURCE_ERROR_NULL_ARGUMENT, 0);
  }
  slice->data = NULL;
  slice->length = 0;
  if (!w_seed_source_validate_span(source, span, error)) return false;
  slice->length = span.end_byte - span.start_byte;
  if (slice->length != 0) {
    slice->data = source->bytes.data + span.start_byte;
  }
  return true;
}

bool w_seed_source_offset_to_point(const w_seed_source *source, size_t offset,
                                   w_seed_source_point *point,
                                   w_seed_source_error *error) {
  clear_error(error);
  if (point == NULL) {
    return fail(error, W_SEED_SOURCE_ERROR_NULL_ARGUMENT, 0);
  }
  point->line = 0;
  point->byte_column = 0;
  if (!source_ready(source, error)) return false;
  if (offset > source->bytes.length) {
    return fail(error, W_SEED_SOURCE_ERROR_OFFSET_OUT_OF_RANGE, offset);
  }
  if (!is_code_point_boundary(source, offset)) {
    return fail(error, W_SEED_SOURCE_ERROR_NOT_CODE_POINT_BOUNDARY, offset);
  }

  size_t start = 0;
  size_t line = 0;
  for (size_t index = 0; index < offset; index += 1) {
    if (source->bytes.data[index] == 0x0Au) {
      line += 1;
      start = index + 1;
    }
  }
  point->line = line;
  point->byte_column = offset - start;
  return true;
}

bool w_seed_source_point_to_offset(const w_seed_source *source,
                                   w_seed_source_point point, size_t *offset,
                                   w_seed_source_error *error) {
  clear_error(error);
  if (offset == NULL) {
    return fail(error, W_SEED_SOURCE_ERROR_NULL_ARGUMENT, 0);
  }
  *offset = 0;
  if (!source_ready(source, error)) return false;
  if (point.line >= source->line_count) {
    return fail(error, W_SEED_SOURCE_ERROR_LINE_OUT_OF_RANGE,
                source->bytes.length);
  }

  const size_t start = line_start(source, point.line);
  const size_t end = line_end(source, start);
  const size_t line_length = end - start;
  if (point.byte_column > line_length) {
    return fail(error, W_SEED_SOURCE_ERROR_COLUMN_OUT_OF_RANGE, end);
  }
  *offset = start + point.byte_column;
  if (!is_code_point_boundary(source, *offset)) {
    return fail(error, W_SEED_SOURCE_ERROR_NOT_CODE_POINT_BOUNDARY, *offset);
  }
  return true;
}
