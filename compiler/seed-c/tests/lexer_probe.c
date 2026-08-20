#include "w_seed_lexer.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

enum { PROBE_LIMIT = 16 * 1024 * 1024, PROBE_MAX_OPAQUE = 16 };

typedef struct {
  size_t start;
  size_t end;
} opaque_arg;

static bool parse_size(const char *text, size_t *value) {
  char *tail = NULL;
  errno = 0;
  const unsigned long long parsed = strtoull(text, &tail, 10);
  if (errno != 0 || tail == text || *tail != '\0' ||
      parsed > (unsigned long long)SIZE_MAX) {
    return false;
  }
  *value = (size_t)parsed;
  return true;
}

static bool parse_opaque(const char *text, opaque_arg *opaque) {
  const char *colon = strchr(text, ':');
  if (colon == NULL) return false;
  const size_t start_length = (size_t)(colon - text);
  if (start_length == 0 || start_length >= 64) return false;
  char start_text[64];
  (void)memcpy(start_text, text, start_length);
  start_text[start_length] = '\0';
  if (!parse_size(start_text, &opaque->start)) return false;
  if (!parse_size(colon + 1, &opaque->end)) return false;
  return opaque->start <= opaque->end;
}

static bool read_stdin(uint8_t **data, size_t *length) {
  size_t capacity = 4096;
  size_t used = 0;
  uint8_t *buffer = (uint8_t *)malloc(capacity);
  if (buffer == NULL) return false;
  while (true) {
    if (used == capacity) {
      if (capacity >= PROBE_LIMIT || capacity > SIZE_MAX / 2) {
        free(buffer);
        return false;
      }
      size_t next_capacity = capacity * 2;
      if (next_capacity > PROBE_LIMIT) next_capacity = PROBE_LIMIT;
      uint8_t *grown = (uint8_t *)realloc(buffer, next_capacity);
      if (grown == NULL) {
        free(buffer);
        return false;
      }
      buffer = grown;
      capacity = next_capacity;
    }
    const size_t room = capacity - used;
    const size_t count = fread(buffer + used, 1, room, stdin);
    used += count;
    if (count < room) {
      if (ferror(stdin) != 0) {
        free(buffer);
        return false;
      }
      break;
    }
  }
  *data = buffer;
  *length = used;
  return true;
}

static int report_error(const w_seed_lex_error *error) {
  (void)fprintf(stderr,
                "lexer error kind=%d primary=%llu:%llu opening=%llu:%llu "
                "literal=%d eof=%d\n",
                error->kind, (unsigned long long)error->primary.start_byte,
                (unsigned long long)error->primary.end_byte,
                (unsigned long long)error->opening.start_byte,
                (unsigned long long)error->opening.end_byte,
                error->literal, error->reached_eof ? 1 : 0);
  return EXIT_FAILURE;
}

int main(int argc, char **argv) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdin), _O_BINARY);
#endif
  opaque_arg opaque[PROBE_MAX_OPAQUE];
  size_t opaque_count = 0;
  for (int index = 1; index < argc; index += 1) {
    const char *argument = argv[index];
    const char *value = NULL;
    if (strncmp(argument, "--opaque=", 9) == 0) {
      value = argument + 9;
    } else if (strcmp(argument, "--opaque") == 0 && index + 1 < argc) {
      index += 1;
      value = argv[index];
    }
    if (value == NULL || opaque_count >= PROBE_MAX_OPAQUE ||
        !parse_opaque(value, &opaque[opaque_count])) {
      (void)fprintf(stderr, "usage: lexer_probe [--opaque=start:end]...\n");
      return EXIT_FAILURE;
    }
    opaque_count += 1;
  }

  uint8_t *data = NULL;
  size_t length = 0;
  if (!read_stdin(&data, &length)) {
    (void)fprintf(stderr, "probe input exceeds 16 MiB or cannot be read\n");
    return EXIT_FAILURE;
  }
  w_seed_source source;
  w_seed_source_error source_error;
  const w_seed_byte_view bytes = {data, length};
  if (!w_seed_source_init(bytes, &source, &source_error)) {
    (void)fprintf(stderr, "source error kind=%d offset=%llu\n",
                  source_error.kind,
                  (unsigned long long)source_error.byte_offset);
    free(data);
    return EXIT_FAILURE;
  }
  w_seed_lexer_frame frames[256];
  w_seed_lexer lexer;
  w_seed_lex_error error;
  const w_seed_span bounds = {0, length};
  if (!w_seed_lexer_init(&source, bounds, frames,
                         sizeof(frames) / sizeof(frames[0]), &lexer, &error)) {
    const int result = report_error(&error);
    free(data);
    return result;
  }

  size_t opaque_index = 0;
  size_t previous_end = 0;
  while (true) {
    if (opaque_index < opaque_count &&
        w_seed_lexer_offset(&lexer) == opaque[opaque_index].start) {
      if (!w_seed_lexer_require_opaque(&lexer, &error) ||
          !w_seed_lexer_claim_opaque(&lexer,
                                     (w_seed_span){opaque[opaque_index].start,
                                                  opaque[opaque_index].end},
                                     &error)) {
        const int result = report_error(&error);
        free(data);
        return result;
      }
      opaque_index += 1;
    }
    w_seed_lex_item item;
    if (!w_seed_lexer_next(&lexer, &item, &error)) {
      const int result = report_error(&error);
      free(data);
      return result;
    }
    if (item.kind == W_SEED_LEX_ITEM_EOF) break;
    if (item.span.start_byte != previous_end ||
        item.span.end_byte < item.span.start_byte ||
        item.span.end_byte > length) {
      (void)fprintf(stderr,
                    "non-lossless item span %llu:%llu after %llu\n",
                    (unsigned long long)item.span.start_byte,
                    (unsigned long long)item.span.end_byte,
                    (unsigned long long)previous_end);
      free(data);
      return EXIT_FAILURE;
    }
    (void)printf("item kind=%d start=%llu end=%llu flags=%u\n", item.kind,
                 (unsigned long long)item.span.start_byte,
                 (unsigned long long)item.span.end_byte,
                 item.kind == W_SEED_LEX_ITEM_NUMBER
                     ? (unsigned int)item.payload.token.flags
                     : 0u);
    previous_end = item.span.end_byte;
  }
  if (previous_end != length || opaque_index != opaque_count) {
    (void)fprintf(stderr, "incomplete partition at %llu of %llu\n",
                  (unsigned long long)previous_end,
                  (unsigned long long)length);
    free(data);
    return EXIT_FAILURE;
  }
  free(data);
  return EXIT_SUCCESS;
}
