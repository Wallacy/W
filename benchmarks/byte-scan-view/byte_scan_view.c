// C23 independent baseline for BMD3 byte-scan-view.
// The program prints no output until the bounded input is fully validated.

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_INPUT_BYTES = 64 * 1024 * 1024, BUFFER_BYTES = 64 * 1024 };

static int parse_delimiter(const char *text, uint8_t *delimiter) {
  char *end = NULL;
  unsigned long value;

  if (text == NULL || text[0] == '\0' || text[0] == '+' || text[0] == '-' ||
      (text[0] == '0' && text[1] != '\0')) return 0;
  errno = 0;
  value = strtoul(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || value > 255UL) return 0;
  *delimiter = (uint8_t)value;
  return 1;
}

int main(int argc, char **argv) {
  FILE *input;
  uint8_t delimiter;
  unsigned char buffer[BUFFER_BYTES];
  uint64_t bytes = 0;
  uint64_t matches = 0;
  size_t count;

  if (argc != 3 || !parse_delimiter(argv[2], &delimiter)) return 2;
  input = fopen(argv[1], "rb");
  if (input == NULL) return 3;

  while ((count = fread(buffer, 1, sizeof(buffer), input)) != 0) {
    size_t index;
    if (bytes > (uint64_t)MAX_INPUT_BYTES - (uint64_t)count) {
      fclose(input);
      return 4;
    }
    bytes += (uint64_t)count;
    for (index = 0; index < count; ++index) {
      if (buffer[index] == delimiter) ++matches;
    }
  }
  if (ferror(input) || fclose(input) != 0) return 5;

  if (printf("{\"bytes\":\"%" PRIu64 "\",\"matches\":\"%" PRIu64 "\"}", bytes, matches) < 0) return 6;
  return 0;
}
