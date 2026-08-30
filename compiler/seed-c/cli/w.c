#include "check.h"
#include "w_cli_io.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *usage_text = "usage: w check <path/file.w> [--json]\n";

static bool write_usage(FILE *stream) {
  return w_seed_cli_write_text(stream, usage_text, &w_seed_cli_stdio_ops);
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
  if (!w_seed_cli_prepare_binary(stdout, &w_seed_cli_stdio_ops)) return 3;
  if (is_help(argc, argv)) return write_usage(stdout) ? 0 : 3;
  const char *path = NULL;
  bool json = false;
  if (!parse_check(argc, argv, &path, &json)) {
    return write_usage(stderr) ? 2 : 3;
  }
  return w_seed_check_run(path, json);
}
