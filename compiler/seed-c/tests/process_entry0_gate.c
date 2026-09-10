#include "w_seed_native0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

static bool logical_source_id(const char *path, size_t length,
                              w_seed_frontend_text *source_id) {
  if (source_id != NULL) *source_id = (w_seed_frontend_text){NULL, 0u};
  if (path == NULL || source_id == NULL || length < 3u || path[0] == '-' ||
      path[length - 2u] != '.' || path[length - 1u] != 'w')
    return false;
  size_t basename_start = 0u;
  for (size_t index = 0u; index < length; index += 1u)
    if (path[index] == '/' || path[index] == '\\') basename_start = index + 1u;
  if (basename_start >= length) return false;
  *source_id = (w_seed_frontend_text){path + basename_start,
                                      length - basename_start};
  return source_id->length <= W_SEED_NATIVE0_MAX_SOURCE_ID_BYTES;
}

static int write_artifact(const uint8_t *bytes, size_t length) {
  if (bytes == NULL || length == 0u ||
      fwrite(bytes, sizeof(uint8_t), length, stdout) != length ||
      fflush(stdout) != 0)
    return 3;
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 2 || argv == NULL || argv[1] == NULL || argv[1][0] == '\0')
    return 2;
#if defined(_WIN32)
  if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 3;
#endif

  const char *path = argv[1];
  const size_t path_length = strlen(path);
  w_seed_frontend_text source_id;
  if (!logical_source_id(path, path_length, &source_id)) return 2;

  w_seed_native0_input input = {
      .path = path,
      .path_length = path_length,
      .logical_source_id = source_id,
      .target = {W_SEED_MLIR0_TARGET_X86_64_PC_WINDOWS_MSVC},
      .artifact_kind = W_SEED_MLIR0_ARTIFACT_PROCESS_HANDLER};
  static w_seed_native0_storage storage;
  static uint8_t artifact[W_SEED_MLIR0_MAX_BYTES];
  const w_seed_native0_output output = {artifact, sizeof(artifact)};
  w_seed_native0_result result;
  const w_seed_native0_status status =
      w_seed_native0_run(&input, &storage, &output, &result);
  if (status != W_SEED_NATIVE0_OK || result.mlir.written.mlir_bytes == 0u)
    return 1;
  return write_artifact(artifact, result.mlir.written.mlir_bytes);
}
