#include "native_benchmark.h"

#include <stdio.h>

#if defined(_WIN32)

int wmain(int argc, wchar_t **argv) {
  if (argc < 1 || argv == NULL) return 2;
  return w_seed_native_benchmark_helper_command(argc - 1, argv + 1);
}

#else

int main(void) {
  (void)fputs("w_seed_native_benchmark: Windows is required\n", stderr);
  return 2;
}

#endif
