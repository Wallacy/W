#include "w_seed_source.h"

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define W_SEED_PROBE_CAPACITY (16u * 1024u * 1024u)

static uint8_t input_bytes[W_SEED_PROBE_CAPACITY];

static const char *error_name(w_seed_source_error_kind kind) {
  switch (kind) {
    case W_SEED_SOURCE_ERROR_NONE:
      return "none";
    case W_SEED_SOURCE_ERROR_NULL_ARGUMENT:
      return "null-argument";
    case W_SEED_SOURCE_ERROR_SOURCE_TOO_LARGE:
      return "source-too-large";
    case W_SEED_SOURCE_ERROR_INVALID_UTF8_LEAD:
      return "invalid-utf8-lead";
    case W_SEED_SOURCE_ERROR_UTF8_STRAY_CONTINUATION:
      return "utf8-stray-continuation";
    case W_SEED_SOURCE_ERROR_UTF8_TRUNCATED:
      return "utf8-truncated";
    case W_SEED_SOURCE_ERROR_UTF8_BAD_CONTINUATION:
      return "utf8-bad-continuation";
    case W_SEED_SOURCE_ERROR_UTF8_OVERLONG:
      return "utf8-overlong";
    case W_SEED_SOURCE_ERROR_UTF8_SURROGATE:
      return "utf8-surrogate";
    case W_SEED_SOURCE_ERROR_UTF8_OUT_OF_RANGE:
      return "utf8-out-of-range";
    case W_SEED_SOURCE_ERROR_SPAN_ORDER:
      return "span-order";
    case W_SEED_SOURCE_ERROR_SPAN_OUT_OF_RANGE:
      return "span-out-of-range";
    case W_SEED_SOURCE_ERROR_NOT_CODE_POINT_BOUNDARY:
      return "not-code-point-boundary";
    case W_SEED_SOURCE_ERROR_OFFSET_OUT_OF_RANGE:
      return "offset-out-of-range";
    case W_SEED_SOURCE_ERROR_LINE_OUT_OF_RANGE:
      return "line-out-of-range";
    case W_SEED_SOURCE_ERROR_COLUMN_OUT_OF_RANGE:
      return "column-out-of-range";
  }
  return "unknown";
}

static int report_error(const char *operation,
                        const w_seed_source_error *error) {
  (void)fprintf(stderr, "error operation=%s kind=%s offset=%" PRIuMAX "\n",
                operation, error_name(error->kind),
                (uintmax_t)error->byte_offset);
  return 2;
}

static int check_point_roundtrip(const w_seed_source *source, size_t offset,
                                 w_seed_source_error *error) {
  w_seed_source_point point = {0};
  if (!w_seed_source_offset_to_point(source, offset, &point, error)) {
    return report_error("offset-to-point", error);
  }
  size_t roundtrip = 0;
  if (!w_seed_source_point_to_offset(source, point, &roundtrip, error)) {
    return report_error("point-roundtrip", error);
  }
  if (roundtrip != offset) {
    (void)fprintf(stderr,
                  "error operation=point-roundtrip kind=source-mismatch "
                  "expected=%" PRIuMAX " actual=%" PRIuMAX "\n",
                  (uintmax_t)offset, (uintmax_t)roundtrip);
    return 2;
  }
  return 0;
}

int main(void) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdin), _O_BINARY);
  (void)_setmode(_fileno(stdout), _O_BINARY);
#endif

  size_t length = 0;
  while (length < W_SEED_PROBE_CAPACITY) {
    const size_t available = W_SEED_PROBE_CAPACITY - length;
    const size_t count = fread(input_bytes + length, 1, available, stdin);
    if (count == 0) {
      if (feof(stdin) != 0) break;
      (void)fputs("error operation=read kind=io offset=0\n", stderr);
      return 2;
    }
    length += count;
    if (ferror(stdin) != 0) {
      (void)fputs("error operation=read kind=io offset=0\n", stderr);
      return 2;
    }
    if (count < available && feof(stdin) != 0) break;
  }
  if (length == W_SEED_PROBE_CAPACITY) {
    const int extra = fgetc(stdin);
    if (extra != EOF) {
      (void)fputs("error operation=read kind=source-too-large offset=", stderr);
      (void)fprintf(stderr, "%" PRIuMAX "\n", (uintmax_t)length);
      return 2;
    }
    if (ferror(stdin) != 0) {
      (void)fputs("error operation=read kind=io offset=0\n", stderr);
      return 2;
    }
  }

  const w_seed_byte_view input = {input_bytes, length};
  w_seed_source source = {0};
  w_seed_source_error error = {0};
  if (!w_seed_source_init(input, &source, &error)) {
    return report_error("init", &error);
  }

  const w_seed_span whole = {0, length};
  if (!w_seed_source_validate_span(&source, whole, &error)) {
    return report_error("whole-span", &error);
  }
  w_seed_byte_view slice = {NULL, 0};
  if (!w_seed_source_slice(&source, whole, &slice, &error)) {
    return report_error("slice", &error);
  }
  if (slice.length != length ||
      (length != 0 && memcmp(slice.data, input_bytes, length) != 0)) {
    (void)fputs("error operation=roundtrip kind=source-mismatch offset=0\n",
                stderr);
    return 2;
  }

  size_t first_line_start = length;
  size_t last_line_start = 0;
  for (size_t index = 0; index < length; index += 1) {
    if (input_bytes[index] == 0x0Au) {
      const size_t next_line = index + 1;
      if (first_line_start == length) first_line_start = next_line;
      last_line_start = next_line;
    }
  }
  if (check_point_roundtrip(&source, 0, &error) != 0) return 2;
  if (first_line_start != length &&
      check_point_roundtrip(&source, first_line_start, &error) != 0) {
    return 2;
  }
  if (last_line_start != 0 &&
      check_point_roundtrip(&source, last_line_start, &error) != 0) {
    return 2;
  }
  if (length != 0 && check_point_roundtrip(&source, length, &error) != 0) {
    return 2;
  }

  if (length != 0 && fwrite(input_bytes, 1, length, stdout) != length) {
    (void)fputs("error operation=write kind=io offset=0\n", stderr);
    return 2;
  }
  (void)fprintf(stderr, "ok length=%" PRIuMAX " bom=%u lines=%" PRIuMAX
                "\n",
                (uintmax_t)length, w_seed_source_has_bom(&source) ? 1u : 0u,
                (uintmax_t)w_seed_source_line_count(&source));
  return 0;
}
