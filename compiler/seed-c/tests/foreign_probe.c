#include "w_seed_foreign.h"

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum { PROBE_CAPACITY = 16 * 1024 * 1024 };

static uint8_t input_bytes[PROBE_CAPACITY];

static bool parse_limit(const char *text, size_t *value) {
  if (text == NULL || value == NULL || *text == '\0') return false;
  char *end = NULL;
  const uintmax_t parsed = strtoumax(text, &end, 10);
  if (end == text || *end != '\0' || parsed > (uintmax_t)SIZE_MAX)
    return false;
  *value = (size_t)parsed;
  return true;
}

static void print_digest(const uint8_t digest[32]) {
  for (size_t index = 0; index < 32; index += 1)
    (void)printf("%02x", (unsigned int)digest[index]);
}

int main(int argc, char **argv) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdin), _O_BINARY);
#endif
  w_seed_foreign_limits limits = {64u * 1024u, 256u};
  for (int index = 1; index < argc; index += 1) {
    const char *argument = argv[index];
    if (strncmp(argument, "--max-body=", 11u) == 0) {
      if (!parse_limit(argument + 11, &limits.maximum_body_bytes)) {
        (void)fputs("error=argument\n", stderr);
        return 2;
      }
    } else if (strncmp(argument, "--max-depth=", 12u) == 0) {
      if (!parse_limit(argument + 12, &limits.maximum_nesting)) {
        (void)fputs("error=argument\n", stderr);
        return 2;
      }
    } else {
      (void)fputs("error=argument\n", stderr);
      return 2;
    }
  }
  size_t length = 0;
  while (length < PROBE_CAPACITY) {
    const size_t room = PROBE_CAPACITY - length;
    const size_t count = fread(input_bytes + length, 1, room, stdin);
    length += count;
    if (count < room) {
      if (ferror(stdin) != 0) {
        (void)fputs("error=read\n", stderr);
        return 2;
      }
      break;
    }
  }
  if (length == PROBE_CAPACITY && fgetc(stdin) != EOF) {
    (void)fputs("error=source-too-large\n", stderr);
    return 2;
  }

  w_seed_foreign_source_validation result;
  w_seed_foreign_error error;
  const bool accepted = w_seed_foreign_scan_c_inline_1(
      (w_seed_byte_view){input_bytes, length}, limits, &result, &error);
  if (accepted) {
    (void)printf("RESULT accepted=1 profile=%d body_start=%" PRIuMAX
                 " body_end=%" PRIuMAX " close=%" PRIuMAX
                 " next=%" PRIuMAX " max_body=%" PRIuMAX
                 " max_depth=%" PRIuMAX " observed_depth=%" PRIuMAX
                 " terminal=%d digest=",
                 result.profile, (uintmax_t)result.body_start_byte,
                 (uintmax_t)result.body_end_byte, (uintmax_t)result.close_byte,
                 (uintmax_t)result.next_byte,
                 (uintmax_t)result.maximum_body_bytes,
                 (uintmax_t)result.maximum_nesting,
                 (uintmax_t)result.maximum_nesting_observed,
                 result.terminal_state);
    print_digest(result.body_digest);
    (void)putchar('\n');
    return 0;
  }
  (void)printf("RESULT accepted=0 kind=%d terminal=%d primary_start=%" PRIuMAX
               " primary_end=%" PRIuMAX " has_close=%d close=%" PRIuMAX
               " max_body=%" PRIuMAX " max_depth=%" PRIuMAX "\n",
               error.kind, error.terminal_state,
               (uintmax_t)error.primary.start_byte,
               (uintmax_t)error.primary.end_byte, error.has_close ? 1 : 0,
               (uintmax_t)error.close_byte,
               (uintmax_t)result.maximum_body_bytes,
               (uintmax_t)result.maximum_nesting);
  return 0;
}
