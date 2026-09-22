#include "native_benchmark.h"
#include "w_cli_io.h"

#include <stdio.h>

int w_seed_cli_main(int argc, char **argv);

#if defined(_WIN32)

#include <windows.h>

#include <stdlib.h>
#include <wchar.h>

static char *argument_to_legacy_bytes(const wchar_t *argument) {
  if (argument == NULL) return NULL;

  const UINT code_page = GetACP();
  BOOL used_default_character = FALSE;
  const DWORD flags = code_page == CP_UTF8 ? WC_ERR_INVALID_CHARS
                                           : WC_NO_BEST_FIT_CHARS;
  BOOL *used_default = code_page == CP_UTF8 ? NULL : &used_default_character;
  const int required = WideCharToMultiByte(code_page, flags, argument, -1,
                                            NULL, 0, NULL, used_default);
  if (required <= 0 || used_default_character) return NULL;

  char *converted = (char *)malloc((size_t)required);
  if (converted == NULL) return NULL;
  used_default_character = FALSE;
  if (WideCharToMultiByte(code_page, flags, argument, -1, converted, required,
                          NULL, used_default) != required ||
      used_default_character) {
    free(converted);
    return NULL;
  }
  return converted;
}

int wmain(int argc, wchar_t **argv) {
  if (argc < 1 || argv == NULL) return 2;
  if (argc >= 3 && wcscmp(argv[1], L"bench") == 0 &&
      wcscmp(argv[2], L"process") == 0) {
    if (!w_seed_cli_prepare_binary(stdout, &w_seed_cli_stdio_ops)) return 3;
    return w_seed_native_benchmark_process_command(argc - 3, argv + 3);
  }

  char **legacy_arguments = (char **)calloc((size_t)argc + 1u,
                                              sizeof(*legacy_arguments));
  if (legacy_arguments == NULL) return 3;
  for (int index = 0; index < argc; index += 1) {
    legacy_arguments[index] = argument_to_legacy_bytes(argv[index]);
    if (legacy_arguments[index] == NULL) {
      for (int cleanup = 0; cleanup < index; cleanup += 1)
        free(legacy_arguments[cleanup]);
      free(legacy_arguments);
      (void)fputs("w: command-line argument is not representable in the "
                  "current Windows code page\n",
                  stderr);
      return 2;
    }
  }
  const int result = w_seed_cli_main(argc, legacy_arguments);
  for (int index = 0; index < argc; index += 1)
    free(legacy_arguments[index]);
  free(legacy_arguments);
  return result;
}

#else

#include <string.h>

int main(int argc, char **argv) {
  if (argc >= 3 && argv != NULL && strcmp(argv[1], "bench") == 0 &&
      strcmp(argv[2], "process") == 0) {
    if (argc == 4 &&
        (strcmp(argv[3], "--help") == 0 || strcmp(argv[3], "-h") == 0)) {
      (void)fputs("usage: w bench process --exe <absolute-path> [options]\n"
                  "  options: --cwd <absolute-dir> --arg <value> --warmup "
                  "<n> --samples <n> --timeout-ms <n> --expect-exit <n> "
                  "--expect-stdout-hex <bytes> --expect-stderr-hex <bytes>\n"
                  "  Windows native cold-process measurements only; no Linux "
                  "backend is available yet\n",
                  stdout);
      return 0;
    }
    (void)fputs("w bench process is currently Windows-only; the Linux native "
                "measurement backend is not implemented\n",
                stderr);
    return 2;
  }
  return w_seed_cli_main(argc, argv);
}

#endif
