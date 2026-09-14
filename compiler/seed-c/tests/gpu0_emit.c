#include "w_seed_gpu0.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * This executable is deliberately a fixture emitter, not a W driver.  The
 * arrays below are the exact bounded witness consumed by GPU0.  Keeping the
 * witness here (rather than synthesizing a similar shape from text) lets the
 * core verifier remain the authority for the two emitted artifacts.
 */
static const w_seed_gpu0_function
    FUNCTIONS[W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY] = {
    {.name = "hostRoot",
     .name_length = 8u,
     .role = W_SEED_GPU0_FUNCTION_HOST_ROOT,
     .first_operation = 0u,
     .operation_count = 6u,
     .parameter_count = 0u,
     .effects = W_SEED_GPU0_EFFECT_NONE},
    {.name = "helloKernel",
     .name_length = 11u,
     .role = W_SEED_GPU0_FUNCTION_DEVICE_KERNEL,
     .first_operation = 6u,
     .operation_count = 1u,
     .parameter_count = 1u,
     .effects = W_SEED_GPU0_EFFECT_NONE},
};

static const w_seed_gpu0_range HOST_RESULT = {
    .address_space = W_SEED_GPU0_ADDRESS_HOST,
    .offset = 0u,
    .bytes = W_SEED_GPU0_RESULT_BYTES,
    .capacity_bytes = W_SEED_GPU0_RESULT_BYTES};
static const w_seed_gpu0_range DEVICE_RESULT = {
    .address_space = W_SEED_GPU0_ADDRESS_DEVICE,
    .offset = 0u,
    .bytes = W_SEED_GPU0_RESULT_BYTES,
    .capacity_bytes = W_SEED_GPU0_RESULT_BYTES};

static const w_seed_gpu0_operation
    OPERATIONS[W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY] = {
    {.kind = W_SEED_GPU0_OPERATION_ALLOCATE_DEVICE_RESULT,
     .source = {0},
     .destination = DEVICE_RESULT,
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = 0},
    {.kind = W_SEED_GPU0_OPERATION_COPY_HOST_TO_DEVICE,
     .source = HOST_RESULT,
     .destination = DEVICE_RESULT,
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = 0},
    {.kind = W_SEED_GPU0_OPERATION_LAUNCH,
     .source = {0},
     .destination = {0},
     .function_index = 1u,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = 0},
    {.kind = W_SEED_GPU0_OPERATION_JOIN,
     .source = {0},
     .destination = {0},
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = 2u,
     .i32_value = 0},
    {.kind = W_SEED_GPU0_OPERATION_COPY_DEVICE_TO_HOST,
     .source = DEVICE_RESULT,
     .destination = HOST_RESULT,
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = 0},
    {.kind = W_SEED_GPU0_OPERATION_VERIFY_RESULT,
     .source = HOST_RESULT,
     .destination = {0},
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = W_SEED_GPU0_EXPECTED_PAYLOAD},
    {.kind = W_SEED_GPU0_OPERATION_STORE_I32,
     .source = {0},
     .destination = DEVICE_RESULT,
     .function_index = W_SEED_GPU0_NONE,
     .dependency_operation = W_SEED_GPU0_NONE,
     .i32_value = W_SEED_GPU0_EXPECTED_PAYLOAD},
};

static w_seed_gpu0_program fixture_program(void) {
  return (w_seed_gpu0_program){
      .functions = FUNCTIONS,
      .function_count = W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY,
      .function_capacity = W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY,
      .operations = OPERATIONS,
      .operation_count = W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY,
      .operation_capacity = W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY,
      .host_result_capacity_bytes = W_SEED_GPU0_RESULT_BYTES,
      .device_result_capacity_bytes = W_SEED_GPU0_RESULT_BYTES};
}

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>

enum {
  GPU0_EMIT_USAGE = 2,
  GPU0_EMIT_CORE = 20,
  GPU0_EMIT_FILE = 21,
  GPU0_EMIT_COMMIT = 22,
};

static void remove_stage(char *path) {
  if (path == NULL) return;
  (void)DeleteFileA(path);
}

static bool path_exists(const char *path) {
  if (path == NULL || path[0] == '\0') return false;
  return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static char *stage_path(const char *destination, const char *tag) {
  if (destination == NULL || tag == NULL) return NULL;
  const size_t destination_bytes = strlen(destination);
  const size_t tag_bytes = strlen(tag);
  const size_t suffix_bytes = strlen(".w-gpu0-stage-0000000000-") +
                              tag_bytes + 1u;
  if (destination_bytes > SIZE_MAX - suffix_bytes) return NULL;
  const size_t bytes = destination_bytes + suffix_bytes;
  char *path = (char *)malloc(bytes);
  if (path == NULL) return NULL;
  const DWORD process_id = GetCurrentProcessId();
  const int written = snprintf(path, bytes, "%s.w-gpu0-stage-%lu-%s",
                               destination, (unsigned long)process_id, tag);
  if (written < 0 || (size_t)written >= bytes) {
    free(path);
    return NULL;
  }
  return path;
}

static bool write_stage(const char *path, const uint8_t *bytes, size_t length) {
  if (path == NULL || bytes == NULL || length == 0u) return false;
  FILE *file = NULL;
  if (fopen_s(&file, path, "wb") != 0 || file == NULL) return false;
  const size_t written = fwrite(bytes, 1u, length, file);
  bool okay = written == length && fflush(file) == 0;
  const int descriptor = _fileno(file);
  if (okay && descriptor >= 0 && _commit(descriptor) != 0) okay = false;
  if (fclose(file) != 0) okay = false;
  if (!okay) remove_stage((char *)path);
  return okay;
}

static bool commit_pair(char *host_stage, const char *host_path,
                        char *device_stage, const char *device_path) {
  if (host_stage == NULL || host_path == NULL || device_stage == NULL ||
      device_path == NULL)
    return false;
  if (!MoveFileExA(host_stage, host_path, MOVEFILE_WRITE_THROUGH)) return false;
  host_stage[0] = '\0';
  if (!MoveFileExA(device_stage, device_path, MOVEFILE_WRITE_THROUGH)) {
    /* Both destinations were checked absent before staging.  Removing the
     * first newly-created destination restores the pre-call state on the
     * ordinary failure path; the error remains nonzero if Windows refuses it.
     */
    (void)DeleteFileA(host_path);
    return false;
  }
  device_stage[0] = '\0';
  return true;
}

static int emit(const char *host_path, const char *device_path) {
  if (host_path == NULL || device_path == NULL || host_path[0] == '\0' ||
      device_path[0] == '\0' || _stricmp(host_path, device_path) == 0) {
    (void)fprintf(stderr,
                  "GPU0 emitter: two distinct explicit output paths are required\n");
    return GPU0_EMIT_USAGE;
  }
  if (path_exists(host_path) || path_exists(device_path)) {
    (void)fprintf(stderr,
                  "GPU0 emitter: output paths must not already exist\n");
    return GPU0_EMIT_FILE;
  }

  uint8_t host_artifact[W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY];
  uint8_t device_artifact[W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY];
  int32_t device_result = -1;
  int32_t host_result = -1;
  w_seed_gpu0_result result;
  const w_seed_gpu0_output output = {
      .host_artifact = host_artifact,
      .host_capacity = sizeof(host_artifact),
      .device_artifact = device_artifact,
      .device_capacity = sizeof(device_artifact),
      .device_result = &device_result,
      .host_result = &host_result};
  const w_seed_gpu0_program program = fixture_program();
  const w_seed_gpu0_status status =
      w_seed_gpu0_run(&program, &output, &result);
  if (status != W_SEED_GPU0_OK ||
      !w_seed_gpu0_verify(&program, &output, &result)) {
    (void)fprintf(stderr, "GPU0 emitter: exact witness rejected (status=%d)\n",
                  (int)status);
    return GPU0_EMIT_CORE + (int)status;
  }

  char *host_stage = stage_path(host_path, "host");
  char *device_stage = stage_path(device_path, "device");
  if (host_stage == NULL || device_stage == NULL ||
      !write_stage(host_stage, host_artifact,
                   result.measurement.host_artifact_bytes) ||
      !write_stage(device_stage, device_artifact,
                   result.measurement.device_artifact_bytes)) {
    (void)fprintf(stderr, "GPU0 emitter: staged artifact write failed\n");
    remove_stage(host_stage);
    remove_stage(device_stage);
    free(host_stage);
    free(device_stage);
    return GPU0_EMIT_FILE;
  }

  const bool committed =
      commit_pair(host_stage, host_path, device_stage, device_path);
  if (!committed) {
    (void)fprintf(stderr, "GPU0 emitter: artifact commit failed\n");
    remove_stage(host_stage);
    remove_stage(device_stage);
    free(host_stage);
    free(device_stage);
    return GPU0_EMIT_COMMIT;
  }
  free(host_stage);
  free(device_stage);
  (void)fprintf(stdout,
                "GPU0 emitter: wrote host/device MLIR to explicit paths (42 witness)\n");
  return 0;
}

int main(int argc, char **argv) {
  const char *host_path = NULL;
  const char *device_path = NULL;
  if (argc == 3) {
    host_path = argv[1];
    device_path = argv[2];
  } else if (argc == 5 &&
             (strcmp(argv[1], "--host") == 0 ||
              strcmp(argv[1], "--host-mlir") == 0) &&
             (strcmp(argv[3], "--device") == 0 ||
              strcmp(argv[3], "--device-mlir") == 0)) {
    host_path = argv[2];
    device_path = argv[4];
  } else {
    (void)fprintf(stderr,
                  "usage: gpu0_emit.exe <host.mlir> <device.mlir>\n"
                  "   or: gpu0_emit.exe --host <host.mlir> --device <device.mlir>\n");
    return GPU0_EMIT_USAGE;
  }
  return emit(host_path, device_path);
}

#else

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  (void)fprintf(stderr, "GPU0 emitter: native Windows only\n");
  return 2;
}

#endif
