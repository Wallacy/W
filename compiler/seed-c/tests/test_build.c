#include "build.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "build parse check failed: %s (%s:%d)\n", #condition, \
                    __FILE__, __LINE__);                                      \
      return false;                                                            \
    }                                                                          \
  } while (0)

static bool rejects(int argc, char **argv) {
  w_seed_build_request request = {"sentinel", "sentinel", "sentinel"};
  CHECK(!w_seed_build_parse(argc, argv, &request));
  CHECK(request.path == NULL && request.target == NULL &&
        request.output == NULL);
  return true;
}

static bool test_valid_requests(void) {
  char *target_first[] = {"w", "build", "restaurant-if.w", "--target",
                          "x86_64-unknown-linux-gnu", "--output", "out",
                          NULL};
  w_seed_build_request request;
  CHECK(w_seed_build_parse(7, target_first, &request));
  CHECK(request.path == target_first[2] &&
        request.target == target_first[4] && request.output == target_first[6]);

  char *output_first[] = {"w", "build", "main.w", "--output", "program",
                          "--target", "x86_64-pc-windows-msvc", NULL};
  CHECK(w_seed_build_parse(7, output_first, &request));
  CHECK(request.path == output_first[2] &&
        request.target == output_first[6] && request.output == output_first[4]);
  return true;
}

static bool test_invalid_requests(void) {
  char *missing_target[] = {"w", "build", "main.w", "--output", "out",
                            "--output", "out2", NULL};
  CHECK(rejects(7, missing_target));
  char *missing_output[] = {"w", "build", "main.w", "--target",
                             "x86_64-unknown-linux-gnu", "--target",
                             "x86_64-pc-windows-msvc", NULL};
  CHECK(rejects(7, missing_output));
  char *duplicate_target[] = {"w", "build", "main.w", "--target",
                              "x86_64-unknown-linux-gnu", "--target",
                              "x86_64-pc-windows-msvc", NULL};
  CHECK(rejects(7, duplicate_target));
  char *wrong_option[] = {"w", "build", "main.w", "--target=",
                          "x86_64-unknown-linux-gnu", "--output", "out",
                          NULL};
  CHECK(rejects(7, wrong_option));
  char *option_value[] = {"w", "build", "main.w", "--target", "--help",
                          "--output", "out", NULL};
  CHECK(rejects(7, option_value));
  char *non_w_path[] = {"w", "build", "main.txt", "--target",
                        "x86_64-unknown-linux-gnu", "--output", "out",
                        NULL};
  CHECK(rejects(7, non_w_path));
  char *extra_argument[] = {"w", "build", "main.w", "--target",
                             "x86_64-unknown-linux-gnu", "--output", "out",
                             "extra", NULL};
  CHECK(rejects(8, extra_argument));
  char *wrong_command[] = {"w", "run", "main.w", "--target",
                            "x86_64-unknown-linux-gnu", "--output", "out",
                            NULL};
  CHECK(rejects(7, wrong_command));
  return true;
}

int main(void) {
  if (!test_valid_requests() || !test_invalid_requests()) return 1;
  (void)puts("seed build: public argument grammar passed");
  return 0;
}
