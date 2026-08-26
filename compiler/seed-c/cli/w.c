#include "check.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static const char *usage_text = "usage: w check <path/file.w> [--json]\n";

static void write_usage(FILE *stream) {
  if (stream != NULL) (void)fputs(usage_text, stream);
}

static bool is_help(int argc, char **argv) {
  if (argc == 2 && (strcmp(argv[1], "--help") == 0 ||
                    strcmp(argv[1], "help") == 0)) {
    return true;
  }
  return argc == 3 && strcmp(argv[1], "check") == 0 &&
         strcmp(argv[2], "--help") == 0;
}

static bool parse_check(int argc, char **argv, const char **path,
                        bool *json) {
  if (argc < 3 || argc > 4 || argv == NULL || path == NULL || json == NULL ||
      strcmp(argv[1], "check") != 0) {
    return false;
  }
  *path = argv[2];
  *json = false;
  if (*path == NULL || (*path)[0] == '\0' || (*path)[0] == '-' ||
      strcmp(*path, "--json") == 0 || strcmp(*path, "--help") == 0) {
    return false;
  }
  if (argc == 4) {
    if (strcmp(argv[3], "--json") != 0) return false;
    *json = true;
  }
  return true;
}

int main(int argc, char **argv) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
  if (is_help(argc, argv)) {
    write_usage(stdout);
    return 0;
  }
  const char *path = NULL;
  bool json = false;
  if (!parse_check(argc, argv, &path, &json)) {
    write_usage(stderr);
    return 2;
  }
  return w_seed_check_run(path, json);
}
