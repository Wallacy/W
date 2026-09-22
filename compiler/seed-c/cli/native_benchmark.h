#ifndef W_SEED_NATIVE_BENCHMARK_CLI_H
#define W_SEED_NATIVE_BENCHMARK_CLI_H

#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Run the `w bench process` command. `argv` contains only command options and
 * retains the Windows wide-character arguments supplied by the native entry
 * point. */
int w_seed_native_benchmark_process_command(int argc, wchar_t **argv);

/* Run the retained standalone helper frontend with its own usage label. */
int w_seed_native_benchmark_helper_command(int argc, wchar_t **argv);

#ifdef __cplusplus
}
#endif

#endif
