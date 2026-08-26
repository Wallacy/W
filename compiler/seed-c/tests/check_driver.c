#include "../cli/check.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static void report_usage(void) {
  (void)fprintf(stderr, "usage: w_seed_check_driver [--json] path/file.w\n");
}

static bool parse_arguments(int argc, char **argv, bool *json,
                            const char **path) {
  if (json == NULL || path == NULL || argv == NULL || argc < 2 || argc > 3)
    return false;
  *json = false;
  *path = NULL;
  if (argc == 2) {
    if (strcmp(argv[1], "--json") == 0) return false;
    *path = argv[1];
    return *path != NULL && (*path)[0] != '\0';
  }
  if (strcmp(argv[1], "--json") == 0) {
    *json = true;
    *path = argv[2];
  } else if (strcmp(argv[2], "--json") == 0) {
    *json = true;
    *path = argv[1];
  } else {
    return false;
  }
  return *path != NULL && (*path)[0] != '\0';
}

int main(int argc, char **argv) {
#ifdef _WIN32
  (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
  bool json = false;
  const char *path = NULL;
  if (!parse_arguments(argc, argv, &json, &path)) {
    report_usage();
    return 2;
  }
  return w_seed_check_run(path, json);
}
