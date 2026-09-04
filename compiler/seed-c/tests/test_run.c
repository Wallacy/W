#include "run.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "run parse check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                      \
      return false;                                                            \
    }                                                                          \
  } while (0)

static bool rejects(int argc, char **argv) {
  w_seed_run_request request = {"sentinel", 7u, NULL};
  CHECK(!w_seed_run_parse(argc, argv, &request));
  CHECK(request.path == NULL && request.argument_count == 0u &&
        request.arguments == NULL);
  return true;
}

static bool test_valid_requests(void) {
  char *basic_argv[] = {"w", "run", "main.w", NULL};
  w_seed_run_request request;
  CHECK(w_seed_run_parse(3, basic_argv, &request));
  CHECK(request.path == basic_argv[2] && request.argument_count == 0u &&
        request.arguments == NULL);

  char *empty_separator_argv[] = {"w", "run", "main.w", "--", NULL};
  CHECK(w_seed_run_parse(4, empty_separator_argv, &request));
  CHECK(request.path == empty_separator_argv[2] &&
        request.argument_count == 0u && request.arguments == NULL);

  char *arguments_argv[] = {"w", "run", "main.w", "--", "a", "--entry",
                             "", NULL};
  CHECK(w_seed_run_parse(7, arguments_argv, &request));
  CHECK(request.argument_count == 3u && request.arguments != NULL);
  CHECK(strcmp(request.arguments[0], "a") == 0 &&
        strcmp(request.arguments[1], "--entry") == 0 &&
        strcmp(request.arguments[2], "") == 0);

  char hyphen_path[] = "restaurant-linear.w";
  char *hyphen_argv[] = {"w", "run", hyphen_path, NULL};
  CHECK(w_seed_run_parse(3, hyphen_argv, &request));
  CHECK(request.path == hyphen_argv[2] && request.argument_count == 0u &&
        request.arguments == NULL);
  return true;
}

static bool test_invalid_requests(void) {
  char *missing_path[] = {"w", "run", NULL};
  CHECK(rejects(2, missing_path));
  char *option_path[] = {"w", "run", "--help", NULL};
  CHECK(rejects(3, option_path));
  char *non_w_path[] = {"w", "run", "main.txt", NULL};
  CHECK(rejects(3, non_w_path));
  char *hyphen_non_w_path[] = {"w", "run", "restaurant-linear.txt", NULL};
  CHECK(rejects(3, hyphen_non_w_path));
  char *extra_without_separator[] = {"w", "run", "main.w", "arg", NULL};
  CHECK(rejects(4, extra_without_separator));
  char *wrong_separator[] = {"w", "run", "main.w", "--entry", "main", NULL};
  CHECK(rejects(5, wrong_separator));
  char *separator_before_path[] = {"w", "run", "--", "main.w", NULL};
  CHECK(rejects(4, separator_before_path));
  return true;
}

int main(void) {
  if (!test_valid_requests() || !test_invalid_requests()) return 1;
  (void)puts("seed run: public argument grammar passed");
  return 0;
}
