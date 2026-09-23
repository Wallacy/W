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
  w_seed_build_request request = {"sentinel", "sentinel", "sentinel",
                                 W_SEED_RUN_COMPILE_PIE_OFF, true};
  CHECK(!w_seed_build_parse(argc, argv, &request));
  CHECK(request.path == NULL && request.target == NULL &&
        request.output == NULL &&
        request.pie_mode == W_SEED_RUN_COMPILE_PIE_ON &&
        !request.pie_mode_explicit);
  return true;
}

static bool test_valid_requests(void) {
  char *target_first[] = {"w", "build", "if.w", "--target",
                          "x86_64-unknown-linux-gnu", "--output", "out",
                          NULL};
  w_seed_build_request request;
  CHECK(w_seed_build_parse(7, target_first, &request));
  CHECK(request.path == target_first[2] &&
        request.target == target_first[4] && request.output == target_first[6] &&
        request.pie_mode == W_SEED_RUN_COMPILE_PIE_ON &&
        !request.pie_mode_explicit);

  char *output_first[] = {"w", "build", "main.w", "--output", "program",
                          "--target", "x86_64-pc-windows-msvc", NULL};
  CHECK(w_seed_build_parse(7, output_first, &request));
  CHECK(request.path == output_first[2] &&
        request.target == output_first[6] && request.output == output_first[4] &&
        request.pie_mode == W_SEED_RUN_COMPILE_PIE_ON &&
        !request.pie_mode_explicit);

  char *pie_on_first[] = {"w", "build", "main.w", "--pie", "on",
                          "--target", "x86_64-unknown-linux-gnu", "--output",
                          "out", NULL};
  CHECK(w_seed_build_parse(9, pie_on_first, &request));
  CHECK(request.pie_mode == W_SEED_RUN_COMPILE_PIE_ON &&
        request.pie_mode_explicit &&
        request.target == pie_on_first[6] && request.output == pie_on_first[8]);

  char *pie_off_last[] = {"w", "build", "main.w", "--target",
                          "x86_64-unknown-linux-gnu", "--output", "out",
                          "--pie", "off", NULL};
  CHECK(w_seed_build_parse(9, pie_off_last, &request));
  CHECK(request.pie_mode == W_SEED_RUN_COMPILE_PIE_OFF &&
        request.pie_mode_explicit &&
        request.target == pie_off_last[4] && request.output == pie_off_last[6]);
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
  char *unknown_pie_mode[] = {"w", "build", "main.w", "--target",
                              "x86_64-unknown-linux-gnu", "--output", "out",
                              "--pie", "auto", NULL};
  CHECK(rejects(9, unknown_pie_mode));
  char *duplicate_pie_mode[] = {"w", "build", "main.w", "--target",
                                "x86_64-unknown-linux-gnu", "--output", "out",
                                "--pie", "on", "--pie", "off", NULL};
  CHECK(rejects(11, duplicate_pie_mode));
  char *pie_off_windows[] = {"w", "build", "main.w", "--target",
                             "x86_64-pc-windows-msvc", "--output", "out",
                             "--pie", "off", NULL};
  CHECK(rejects(9, pie_off_windows));
  char *pie_on_windows[] = {"w", "build", "main.w", "--target",
                            "x86_64-pc-windows-msvc", "--output", "out",
                            "--pie", "on", NULL};
  CHECK(rejects(9, pie_on_windows));
  char *pie_off_other_target[] = {"w", "build", "main.w", "--target",
                                  "x86_64-unknown-freebsd", "--output", "out",
                                  "--pie", "off", NULL};
  CHECK(rejects(9, pie_off_other_target));
  char *pie_on_other_target[] = {"w", "build", "main.w", "--target",
                                 "x86_64-unknown-freebsd", "--output", "out",
                                 "--pie", "on", NULL};
  CHECK(rejects(9, pie_on_other_target));
  return true;
}

static bool test_execute_fails_closed(void) {
  const w_seed_build_request explicit_windows_pie = {
      "main.w", W_SEED_NATIVE_TARGET_WINDOWS, "unused",
      W_SEED_RUN_COMPILE_PIE_ON, true};
  CHECK(w_seed_build_execute(&explicit_windows_pie) == 2);
  const w_seed_build_request windows_non_pie = {
      "main.w", W_SEED_NATIVE_TARGET_WINDOWS, "unused",
      W_SEED_RUN_COMPILE_PIE_OFF, false};
  CHECK(w_seed_build_execute(&windows_non_pie) == 2);
  const w_seed_build_request invalid_mode = {
      "main.w", W_SEED_NATIVE_TARGET_LINUX, "unused",
      (w_seed_run_compile_pie_mode)2, true};
  CHECK(w_seed_build_execute(&invalid_mode) == 2);
  return true;
}

int main(void) {
  if (!test_valid_requests() || !test_invalid_requests() ||
      !test_execute_fails_closed())
    return 1;
  (void)puts("seed build: public argument grammar passed");
  return 0;
}
