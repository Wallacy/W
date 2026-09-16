/*
 * GPU0's deliberately small Windows Driver adapter.
 *
 * This file does not include CUDA headers and does not link an import
 * library. The CUDA Driver ABI declarations below are the only provider
 * surface used by this experimental probe. Normal execution crosses the
 * private ACCPROV0 boundary; the benchmark path remains a diagnostic probe
 * over the same provider primitives and is not semantic W output.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "w_seed_accelerated_provider0.h"
#include "w_seed_sha256.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>

enum {
  GPU0_CUDA_FAILURE = 2,
  GPU0_CUDA_SUCCESS = 0,
  GPU0_CUDA_MAX_PTX_BYTES = 16 * 1024 * 1024,
  GPU0_CUDA_MAX_METADATA_BYTES = 64 * 1024,
  GPU0_CUDA_MAX_REQUEST_TEXT_BYTES = 512,
  GPU0_CUDA_MAX_BENCHMARK_ITERATIONS = 10001,
};

static const char GPU0_CUDA_IMPLEMENTATION[] =
    "cuda-driver-dynamic-windows";

typedef struct {
  int64_t h2d_ticks;
  int64_t dispatch_sync_ticks;
  int64_t d2h_ticks;
  int64_t end_to_end_ticks;
} gpu0_cuda_timing;

typedef int CUresult;
typedef int CUdevice;
typedef uint64_t CUdeviceptr;
typedef struct CUctx_st *CUcontext;
typedef struct CUmod_st *CUmodule;
typedef struct CUfunc_st *CUfunction;
typedef struct CUstream_st *CUstream;

#define GPU0_CUDA_API __stdcall

typedef CUresult(GPU0_CUDA_API *gpu0_cuInit)(unsigned int flags);
typedef CUresult(GPU0_CUDA_API *gpu0_cuDeviceGet)(CUdevice *device,
                                                   int ordinal);
typedef CUresult(GPU0_CUDA_API *gpu0_cuCtxCreate_v2)(CUcontext *context,
                                                      unsigned int flags,
                                                      CUdevice device);
typedef CUresult(GPU0_CUDA_API *gpu0_cuCtxDestroy_v2)(CUcontext context);
typedef CUresult(GPU0_CUDA_API *gpu0_cuModuleLoadData)(CUmodule *module,
                                                        const void *image);
typedef CUresult(GPU0_CUDA_API *gpu0_cuModuleUnload)(CUmodule module);
typedef CUresult(GPU0_CUDA_API *gpu0_cuModuleGetFunction)(
    CUfunction *function, CUmodule module, const char *name);
typedef CUresult(GPU0_CUDA_API *gpu0_cuMemAlloc_v2)(CUdeviceptr *device,
                                                    size_t bytes);
typedef CUresult(GPU0_CUDA_API *gpu0_cuMemFree_v2)(CUdeviceptr device);
typedef CUresult(GPU0_CUDA_API *gpu0_cuMemcpyHtoD_v2)(CUdeviceptr destination,
                                                       const void *source,
                                                       size_t bytes);
typedef CUresult(GPU0_CUDA_API *gpu0_cuMemcpyDtoH_v2)(void *destination,
                                                       CUdeviceptr source,
                                                       size_t bytes);
typedef CUresult(GPU0_CUDA_API *gpu0_cuLaunchKernel)(
    CUfunction function, unsigned int grid_x, unsigned int grid_y,
    unsigned int grid_z, unsigned int block_x, unsigned int block_y,
    unsigned int block_z, unsigned int shared_bytes, CUstream stream,
    void **kernel_parameters, void **extra);
typedef CUresult(GPU0_CUDA_API *gpu0_cuCtxSynchronize)(void);

typedef struct {
  gpu0_cuInit cuInit;
  gpu0_cuDeviceGet cuDeviceGet;
  gpu0_cuCtxCreate_v2 cuCtxCreate_v2;
  gpu0_cuCtxDestroy_v2 cuCtxDestroy_v2;
  gpu0_cuModuleLoadData cuModuleLoadData;
  gpu0_cuModuleUnload cuModuleUnload;
  gpu0_cuModuleGetFunction cuModuleGetFunction;
  gpu0_cuMemAlloc_v2 cuMemAlloc_v2;
  gpu0_cuMemFree_v2 cuMemFree_v2;
  gpu0_cuMemcpyHtoD_v2 cuMemcpyHtoD_v2;
  gpu0_cuMemcpyDtoH_v2 cuMemcpyDtoH_v2;
  gpu0_cuLaunchKernel cuLaunchKernel;
  gpu0_cuCtxSynchronize cuCtxSynchronize;
} gpu0_cuda_api;

typedef struct {
  int32_t expected;
  size_t device_artifact_bytes;
  char kernel[W_SEED_ACCELERATED_PROVIDER0_MAX_IDENTITY_BYTES];
  uint8_t request_semantic_digest[32];
  uint8_t request_provenance_digest[32];
  uint8_t binding_semantic_digest[32];
  uint8_t binding_provenance_digest[32];
  uint8_t gpu_semantic_identity[32];
  uint8_t device_artifact_identity_digest[32];
  uint8_t device_artifact_digest[32];
} gpu0_request_metadata;

typedef struct {
  w_seed_accelerated_request0_record request;
  uint8_t text[GPU0_CUDA_MAX_REQUEST_TEXT_BYTES];
  uint8_t *device_artifact;
  size_t device_artifact_capacity;
  w_seed_accelerated_request0_program program;
  w_seed_accelerated_request0_result result;
} gpu0_request;

typedef struct {
  const char *provider_path;
  const uint8_t *ptx;
  const char *kernel_name;
  uint32_t generation;
  HMODULE library;
  gpu0_cuda_api api;
  CUdevice device;
  CUcontext context;
  CUmodule module;
  CUfunction function;
  CUdeviceptr device_result;
  int32_t host_result;
  bool effect_started;
} gpu0_cuda_context;

static void report_message(const char *message) {
  if (message != NULL)
    (void)fprintf(stderr, "GPU0 CUDA adapter: %s\n", message);
}

static void report_status(const char *operation, uint32_t status) {
  (void)fprintf(stderr, "GPU0 CUDA adapter: %s failed (status %lu)\n",
                operation == NULL ? "provider boundary" : operation,
                (unsigned long)status);
}

static void report_cuda(const char *operation, CUresult status) {
  report_status(operation, (uint32_t)status);
}

static bool load_symbol(HMODULE library, const char *name, void *destination,
                        size_t destination_bytes) {
  if (library == NULL || name == NULL || destination == NULL ||
      destination_bytes != sizeof(FARPROC))
    return false;
  FARPROC symbol = GetProcAddress(library, name);
  if (symbol == NULL) return false;
  (void)memcpy(destination, &symbol, sizeof(symbol));
  return true;
}

static bool load_api(HMODULE library, gpu0_cuda_api *api,
                     const char **missing_name) {
  if (library == NULL || api == NULL || missing_name == NULL) return false;
  (void)memset(api, 0, sizeof(*api));

#define GPU0_LOAD(field, spelling)                                             \
  do {                                                                          \
    if (!load_symbol(library, spelling, &api->field, sizeof(api->field))) {    \
      *missing_name = spelling;                                                 \
      return false;                                                             \
    }                                                                           \
  } while (0)

  GPU0_LOAD(cuInit, "cuInit");
  GPU0_LOAD(cuDeviceGet, "cuDeviceGet");
  GPU0_LOAD(cuCtxCreate_v2, "cuCtxCreate_v2");
  GPU0_LOAD(cuCtxDestroy_v2, "cuCtxDestroy_v2");
  GPU0_LOAD(cuModuleLoadData, "cuModuleLoadData");
  GPU0_LOAD(cuModuleUnload, "cuModuleUnload");
  GPU0_LOAD(cuModuleGetFunction, "cuModuleGetFunction");
  GPU0_LOAD(cuMemAlloc_v2, "cuMemAlloc_v2");
  GPU0_LOAD(cuMemFree_v2, "cuMemFree_v2");
  GPU0_LOAD(cuMemcpyHtoD_v2, "cuMemcpyHtoD_v2");
  GPU0_LOAD(cuMemcpyDtoH_v2, "cuMemcpyDtoH_v2");
  GPU0_LOAD(cuLaunchKernel, "cuLaunchKernel");
  GPU0_LOAD(cuCtxSynchronize, "cuCtxSynchronize");

#undef GPU0_LOAD
  *missing_name = NULL;
  return true;
}

static bool read_binary(const char *path, size_t maximum, uint8_t **bytes_out,
                        size_t *size_out) {
  if (path == NULL || path[0] == '\0' || bytes_out == NULL ||
      size_out == NULL || maximum == 0u)
    return false;
  *bytes_out = NULL;
  *size_out = 0u;

  FILE *file = NULL;
  if (fopen_s(&file, path, "rb") != 0 || file == NULL) return false;
  bool okay = _fseeki64(file, 0, SEEK_END) == 0;
  const int64_t signed_size = okay ? (int64_t)_ftelli64(file) : -1;
  okay = okay && signed_size > 0 && signed_size <= (int64_t)maximum &&
         _fseeki64(file, 0, SEEK_SET) == 0;
  if (!okay) {
    (void)fclose(file);
    return false;
  }

  const size_t size = (size_t)signed_size;
  if (size > SIZE_MAX - 1u) {
    (void)fclose(file);
    return false;
  }
  uint8_t *bytes = (uint8_t *)malloc(size + 1u);
  if (bytes == NULL) {
    (void)fclose(file);
    return false;
  }
  const size_t read_count = fread(bytes, 1u, size, file);
  const bool closed = fclose(file) == 0;
  if (read_count != size || !closed) {
    free(bytes);
    return false;
  }
  bytes[size] = 0u;
  *bytes_out = bytes;
  *size_out = size;
  return true;
}

static bool parse_expected_i32(const char *text, int32_t *value) {
  if (text == NULL || value == NULL || text[0] == '\0') return false;
  const char *cursor = text;
  const bool negative = *cursor == '-';
  if (negative && *++cursor == '\0') return false;
  if (*cursor == '0' && cursor[1] != '\0') return false;
  const uint32_t limit = negative ? UINT32_C(2147483648) :
                                    UINT32_C(2147483647);
  uint32_t magnitude = 0u;
  do {
    if (*cursor < '0' || *cursor > '9') return false;
    const uint32_t digit = (uint32_t)(*cursor - '0');
    if (magnitude > (limit - digit) / 10u) return false;
    magnitude = magnitude * 10u + digit;
    cursor += 1;
  } while (*cursor != '\0');
  if (negative && magnitude == 0u) return false;
  if (negative && magnitude == UINT32_C(2147483648))
    *value = INT32_MIN;
  else if (negative)
    *value = -(int32_t)magnitude;
  else
    *value = (int32_t)magnitude;
  return true;
}

static bool parse_size(const char *text, size_t *value) {
  if (text == NULL || value == NULL || text[0] == '\0') return false;
  size_t parsed = 0u;
  const char *cursor = text;
  if (*cursor == '0' && cursor[1] != '\0') return false;
  do {
    if (*cursor < '0' || *cursor > '9') return false;
    const size_t digit = (size_t)(*cursor - '0');
    if (parsed > (SIZE_MAX - digit) / 10u) return false;
    parsed = parsed * 10u + digit;
    cursor += 1;
  } while (*cursor != '\0');
  *value = parsed;
  return true;
}

static bool json_string(const char *json, const char *key,
                        const char **value, size_t *bytes) {
  if (json == NULL || key == NULL || value == NULL || bytes == NULL)
    return false;
  const size_t key_bytes = strlen(key);
  char needle[96];
  if (key_bytes > sizeof(needle) - 5u) return false;
  needle[0] = '"';
  (void)memcpy(needle + 1u, key, key_bytes);
  needle[key_bytes + 1u] = '"';
  needle[key_bytes + 2u] = ':';
  needle[key_bytes + 3u] = '"';
  needle[key_bytes + 4u] = '\0';
  const char *match = strstr(json, needle);
  if (match == NULL || strstr(match + 1u, needle) != NULL) return false;
  const char *start = match + key_bytes + 4u;
  const char *end = strchr(start, '"');
  if (end == NULL || end == start) return false;
  for (const char *cursor = start; cursor < end; cursor += 1)
    if ((unsigned char)*cursor < 0x20u || *cursor == '\\') return false;
  *value = start;
  *bytes = (size_t)(end - start);
  return true;
}

static bool json_number(const char *json, const char *key,
                        const char **value, size_t *bytes) {
  if (json == NULL || key == NULL || value == NULL || bytes == NULL)
    return false;
  const size_t key_bytes = strlen(key);
  char needle[96];
  if (key_bytes > sizeof(needle) - 4u) return false;
  needle[0] = '"';
  (void)memcpy(needle + 1u, key, key_bytes);
  needle[key_bytes + 1u] = '"';
  needle[key_bytes + 2u] = ':';
  needle[key_bytes + 3u] = '\0';
  const char *match = strstr(json, needle);
  if (match == NULL || strstr(match + 1u, needle) != NULL) return false;
  const char *start = match + key_bytes + 3u;
  const char *end = start;
  while (*end != '\0' && *end != ',' && *end != '}' && *end != ']' &&
         *end != '\r' && *end != '\n' && *end != ' ' && *end != '\t')
    end += 1;
  if (end == start) return false;
  for (const char *cursor = start; cursor < end; cursor += 1)
    if ((*cursor < '0' || *cursor > '9') && *cursor != '-') return false;
  *value = start;
  *bytes = (size_t)(end - start);
  return true;
}

static bool json_i32(const char *json, const char *key, int32_t *value) {
  const char *text = NULL;
  size_t bytes = 0u;
  char buffer[32];
  if (!json_number(json, key, &text, &bytes) || bytes >= sizeof(buffer))
    return false;
  (void)memcpy(buffer, text, bytes);
  buffer[bytes] = '\0';
  return parse_expected_i32(buffer, value);
}

static bool json_size(const char *json, const char *key, size_t *value) {
  const char *text = NULL;
  size_t bytes = 0u;
  char buffer[32];
  if (!json_number(json, key, &text, &bytes) || bytes >= sizeof(buffer))
    return false;
  (void)memcpy(buffer, text, bytes);
  buffer[bytes] = '\0';
  return parse_size(buffer, value);
}

static uint8_t hex_nibble(char value) {
  if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
  if (value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
  return UINT8_MAX;
}

static bool json_digest(const char *json, const char *key, uint8_t digest[32]) {
  const char *text = NULL;
  size_t bytes = 0u;
  if (!json_string(json, key, &text, &bytes) || bytes != 71u ||
      memcmp(text, "sha256:", 7u) != 0 || digest == NULL)
    return false;
  for (size_t index = 0u; index < 32u; index += 1u) {
    const uint8_t high = hex_nibble(text[7u + index * 2u]);
    const uint8_t low = hex_nibble(text[8u + index * 2u]);
    if (high == UINT8_MAX || low == UINT8_MAX) return false;
    digest[index] = (uint8_t)((high << 4u) | low);
  }
  return true;
}

static bool parse_metadata(const uint8_t *bytes, size_t byte_count,
                           gpu0_request_metadata *metadata) {
  if (bytes == NULL || byte_count == 0u || metadata == NULL ||
      memchr(bytes, 0, byte_count) != NULL)
    return false;
  const char *json = (const char *)bytes;
  const char *schema = NULL;
  size_t schema_bytes = 0u;
  const char *kernel = NULL;
  size_t kernel_bytes = 0u;
  (void)memset(metadata, 0, sizeof(*metadata));
  if (!json_string(json, "schema", &schema, &schema_bytes) ||
      schema_bytes != sizeof(W_SEED_ACCELERATED_REQUEST0_SCHEMA_VERSION) - 1u ||
      memcmp(schema, W_SEED_ACCELERATED_REQUEST0_SCHEMA_VERSION,
             schema_bytes) != 0 ||
      !json_string(json, "kernel", &kernel, &kernel_bytes) ||
      kernel_bytes == 0u || kernel_bytes >= sizeof(metadata->kernel) ||
      !json_i32(json, "expected", &metadata->expected) ||
      !json_size(json, "deviceArtifactBytes", &metadata->device_artifact_bytes) ||
      metadata->device_artifact_bytes == 0u ||
      metadata->device_artifact_bytes > W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY ||
      !json_digest(json, "requestSemanticDigest", metadata->request_semantic_digest) ||
      !json_digest(json, "requestProvenanceDigest", metadata->request_provenance_digest) ||
      !json_digest(json, "bindingSemanticDigest", metadata->binding_semantic_digest) ||
      !json_digest(json, "bindingProvenanceDigest", metadata->binding_provenance_digest) ||
      !json_digest(json, "gpuSemanticIdentity", metadata->gpu_semantic_identity) ||
      !json_digest(json, "deviceArtifactIdentityDigest", metadata->device_artifact_identity_digest) ||
      !json_digest(json, "deviceArtifactDigest", metadata->device_artifact_digest))
    return false;
  (void)memcpy(metadata->kernel, kernel, kernel_bytes);
  metadata->kernel[kernel_bytes] = '\0';
  return true;
}

static bool append_request_text(gpu0_request *request, const char *text,
                                size_t bytes, size_t *offset,
                                size_t *field_bytes) {
  if (request == NULL || text == NULL || bytes == 0u || offset == NULL ||
      field_bytes == NULL || request->program.text_bytes > sizeof(request->text) ||
      bytes > sizeof(request->text) - request->program.text_bytes)
    return false;
  *offset = request->program.text_bytes;
  *field_bytes = bytes;
  (void)memcpy(request->text + request->program.text_bytes, text, bytes);
  request->program.text_bytes += bytes;
  return true;
}

static void hash_u32(w_seed_sha256_state *state, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8u),
                            (uint8_t)(value >> 16u), (uint8_t)(value >> 24u)};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void hash_u64(w_seed_sha256_state *state, uint64_t value) {
  uint8_t bytes[8];
  for (size_t index = 0u; index < sizeof(bytes); index += 1u)
    bytes[index] = (uint8_t)(value >> (index * 8u));
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void hash_bytes(w_seed_sha256_state *state, const uint8_t *bytes,
                       size_t count) {
  hash_u64(state, (uint64_t)count);
  if (count != 0u) w_seed_sha256_update(state, bytes, count);
}

static void request_hashes(const w_seed_accelerated_request0_record *record,
                           const uint8_t *text,
                           uint8_t semantic[32], uint8_t provenance[32]) {
  const size_t offsets[] = {
      record->root_offset, record->domain_offset,
      record->kernel_contract_name_offset, record->module_identity_offset,
      record->artifact_identity_offset, record->kernel_label_offset,
      record->function_name_offset, record->kernel_instance_offset,
      record->target_offset, record->provider_class_offset,
      record->kernel_symbol_offset, record->queue_offset, record->device_offset,
      record->generation_offset,
  };
  const size_t lengths[] = {
      record->root_bytes, record->domain_bytes,
      record->kernel_contract_name_bytes, record->module_identity_bytes,
      record->artifact_identity_bytes, record->kernel_label_bytes,
      record->function_name_bytes, record->kernel_instance_bytes,
      record->target_bytes, record->provider_class_bytes,
      record->kernel_symbol_bytes, record->queue_bytes, record->device_bytes,
      record->generation_bytes,
  };
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  static const char semantic_tag[] =
      "w-seed-accelerated-request0-semantic-2";
  w_seed_sha256_update(&state, (const uint8_t *)semantic_tag,
                       sizeof(semantic_tag) - 1u);
  for (size_t index = 0u; index < 11u; index += 1u) {
    hash_u64(&state, (uint64_t)lengths[index]);
    w_seed_sha256_update(&state, text + offsets[index], lengths[index]);
  }
  hash_u64(&state, (uint64_t)record->device_artifact_bytes);
  hash_u32(&state, record->result_bytes);
  hash_u32(&state, record->result_bit_width);
  hash_u32(&state, record->result_is_signed ? 1u : 0u);
  hash_u32(&state, (uint32_t)record->sentinel_expected_i32);
  w_seed_sha256_update(&state, record->binding_semantic_digest, 32u);
  w_seed_sha256_update(&state, record->gpu_semantic_identity, 32u);
  w_seed_sha256_update(&state, record->device_artifact_identity_digest, 32u);
  w_seed_sha256_update(&state, record->device_artifact_digest, 32u);
  w_seed_sha256_final(&state, semantic);

  w_seed_sha256_init(&state);
  static const char provenance_tag[] =
      "w-seed-accelerated-request0-provenance-2";
  w_seed_sha256_update(&state, (const uint8_t *)provenance_tag,
                       sizeof(provenance_tag) - 1u);
  w_seed_sha256_update(&state, semantic, 32u);
  for (size_t index = 11u; index < 14u; index += 1u) {
    hash_u64(&state, (uint64_t)lengths[index]);
    w_seed_sha256_update(&state, text + offsets[index], lengths[index]);
  }
  w_seed_sha256_update(&state, record->binding_provenance_digest, 32u);
  w_seed_sha256_final(&state, provenance);
}

static bool make_request(uint8_t *device_artifact, size_t device_artifact_bytes,
                         const gpu0_request_metadata *metadata,
                         gpu0_request *request) {
  if (device_artifact == NULL || device_artifact_bytes == 0u ||
      device_artifact_bytes > W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY ||
      metadata == NULL || request == NULL ||
      device_artifact_bytes != metadata->device_artifact_bytes)
    return false;
  (void)memset(request, 0, sizeof(*request));
  request->device_artifact = device_artifact;
  request->device_artifact_capacity = device_artifact_bytes;
  request->program.requests = &request->request;
  request->program.request_count = 1u;
  request->program.request_capacity = 1u;
  request->program.text = request->text;
  request->program.text_capacity = sizeof(request->text);
  request->program.device_artifact = device_artifact;
  request->program.device_artifact_bytes = device_artifact_bytes;
  request->program.device_artifact_capacity = device_artifact_bytes;

  static const char root[] = "root:main";
  static const char domain[] = "inference";
  static const char contract[] = "kernels";
  static const char module[] = "example.kernels@1";
  static const char artifact[] = "artifact:gpu0";
  static const char label[] = "hello";
  static const char function[] = "kernel";
  static const char instance[] = "instance:hello:0";
  static const char target[] = "nvptx64-nvidia-cuda";
  static const char provider[] = "cuda";
  static const char queue[] = "queue:0";
  static const char device[] = "device:0";
  static const char generation[] = "generation:1";
  w_seed_accelerated_request0_record *record = &request->request;
  if (!append_request_text(request, root, sizeof(root) - 1u,
                           &record->root_offset, &record->root_bytes) ||
      !append_request_text(request, domain, sizeof(domain) - 1u,
                           &record->domain_offset, &record->domain_bytes) ||
      !append_request_text(request, contract, sizeof(contract) - 1u,
                           &record->kernel_contract_name_offset,
                           &record->kernel_contract_name_bytes) ||
      !append_request_text(request, module, sizeof(module) - 1u,
                           &record->module_identity_offset,
                           &record->module_identity_bytes) ||
      !append_request_text(request, artifact, sizeof(artifact) - 1u,
                           &record->artifact_identity_offset,
                           &record->artifact_identity_bytes) ||
      !append_request_text(request, label, sizeof(label) - 1u,
                           &record->kernel_label_offset,
                           &record->kernel_label_bytes) ||
      !append_request_text(request, function, sizeof(function) - 1u,
                           &record->function_name_offset,
                           &record->function_name_bytes) ||
      !append_request_text(request, instance, sizeof(instance) - 1u,
                           &record->kernel_instance_offset,
                           &record->kernel_instance_bytes) ||
      !append_request_text(request, target, sizeof(target) - 1u,
                           &record->target_offset, &record->target_bytes) ||
      !append_request_text(request, provider, sizeof(provider) - 1u,
                           &record->provider_class_offset,
                           &record->provider_class_bytes) ||
      !append_request_text(request, metadata->kernel, strlen(metadata->kernel),
                           &record->kernel_symbol_offset,
                           &record->kernel_symbol_bytes) ||
      !append_request_text(request, queue, sizeof(queue) - 1u,
                           &record->queue_offset, &record->queue_bytes) ||
      !append_request_text(request, device, sizeof(device) - 1u,
                           &record->device_offset, &record->device_bytes) ||
      !append_request_text(request, generation, sizeof(generation) - 1u,
                           &record->generation_offset,
                           &record->generation_bytes))
    return false;

  record->device_artifact_bytes = device_artifact_bytes;
  record->result_bytes = sizeof(int32_t);
  record->result_bit_width = 32u;
  record->result_is_signed = true;
  record->sentinel_expected_i32 = metadata->expected;
  (void)memcpy(record->binding_semantic_digest,
               metadata->binding_semantic_digest, 32u);
  (void)memcpy(record->binding_provenance_digest,
               metadata->binding_provenance_digest, 32u);
  (void)memcpy(record->gpu_semantic_identity, metadata->gpu_semantic_identity,
               32u);
  (void)memcpy(record->device_artifact_identity_digest,
               metadata->device_artifact_identity_digest, 32u);
  (void)memcpy(record->device_artifact_digest,
               metadata->device_artifact_digest, 32u);
  request_hashes(record, request->text, request->program.semantic_digest,
                 request->program.provenance_digest);

  (void)memset(&request->result, 0, sizeof(request->result));
  request->result.status = W_SEED_ACCELERATED_REQUEST0_OK;
  request->result.required =
      (w_seed_accelerated_request0_counts){1u, request->program.text_bytes,
                                           device_artifact_bytes};
  request->result.written = request->result.required;
  (void)memcpy(request->result.schema,
               W_SEED_ACCELERATED_REQUEST0_SCHEMA_VERSION,
               sizeof(request->result.schema));
  (void)memcpy(request->result.binding_schema,
               W_SEED_ACCELERATED_BINDING0_SCHEMA_VERSION,
               sizeof(request->result.binding_schema));
  (void)memcpy(request->result.gpu_schema, W_SEED_GPU0_SCHEMA_VERSION,
               sizeof(request->result.gpu_schema));
  (void)memcpy(request->result.semantic_digest,
               metadata->request_semantic_digest, 32u);
  (void)memcpy(request->result.provenance_digest,
               metadata->request_provenance_digest, 32u);
  return w_seed_accelerated_request0_verify(&request->program,
                                            &request->result);
}

static bool request_text_view(
    const w_seed_accelerated_request0_program *program, size_t offset,
    size_t bytes, const char **text) {
  if (program == NULL || text == NULL || program->text == NULL ||
      offset > program->text_bytes || bytes > program->text_bytes - offset ||
      bytes == 0u)
    return false;
  *text = (const char *)program->text + offset;
  return true;
}

static void digest_text(const char *text, size_t bytes, uint8_t digest[32]) {
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  hash_bytes(&state, (const uint8_t *)text, bytes);
  w_seed_sha256_final(&state, digest);
}

static bool make_artifact_receipt(
    const w_seed_accelerated_request0_program *request,
    const uint8_t *native_bytes, size_t native_byte_count,
    w_seed_accelerated_provider0_artifact_receipt *receipt) {
  if (request == NULL || request->requests == NULL ||
      request->request_count != 1u || native_bytes == NULL ||
      native_byte_count == 0u || receipt == NULL)
    return false;
  const w_seed_accelerated_request0_record *record = &request->requests[0];
  const char *target = NULL;
  const char *provider = NULL;
  if (!request_text_view(request, record->target_offset, record->target_bytes,
                         &target) ||
      !request_text_view(request, record->provider_class_offset,
                         record->provider_class_bytes, &provider))
    return false;
  (void)memset(receipt, 0, sizeof(*receipt));
  (void)memcpy(receipt->schema,
               W_SEED_ACCELERATED_PROVIDER0_ARTIFACT_RECEIPT_SCHEMA_VERSION,
               sizeof(receipt->schema));
  (void)memcpy(receipt->request_device_artifact_digest,
               record->device_artifact_digest, 32u);
  digest_text(target, record->target_bytes, receipt->target_digest);
  digest_text(provider, record->provider_class_bytes,
              receipt->provider_abi_class_digest);
  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, native_bytes, native_byte_count);
  w_seed_sha256_final(&state, receipt->native_artifact_digest);
  static const char link_tag[] =
      "w-seed-accelerated-native-artifact-link-1";
  w_seed_sha256_init(&state);
  w_seed_sha256_update(&state, (const uint8_t *)link_tag,
                       sizeof(link_tag) - 1u);
  w_seed_sha256_update(&state, receipt->request_device_artifact_digest, 32u);
  w_seed_sha256_update(&state, receipt->target_digest, 32u);
  w_seed_sha256_update(&state, receipt->provider_abi_class_digest, 32u);
  w_seed_sha256_update(&state, receipt->native_artifact_digest, 32u);
  w_seed_sha256_final(&state, receipt->link_digest);
  return true;
}

static void copy_event_text(char destination[], size_t capacity,
                            const char *source) {
  if (destination == NULL || capacity == 0u || source == NULL) return;
  const size_t bytes = strlen(source);
  if (bytes >= capacity) return;
  (void)memcpy(destination, source, bytes);
}

static void set_event_identity(const gpu0_cuda_context *context,
                               w_seed_accelerated_provider0_callback_event *event) {
  if (context == NULL || event == NULL) return;
  event->generation = context->generation;
  copy_event_text(event->device, sizeof(event->device), "device:0");
  copy_event_text(event->queue, sizeof(event->queue), "queue:0");
  copy_event_text(event->generation_text, sizeof(event->generation_text),
                  "generation:1");
}

static void event_cuda_failure(
    w_seed_accelerated_provider0_callback_event *event, CUresult status) {
  if (event == NULL) return;
  event->status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_FAILURE;
  event->raw_status = (uint32_t)status;
}

static bool cuda_stage(
    void *raw, const w_seed_accelerated_request0_program *request_program,
    const w_seed_accelerated_request0_result *request_result,
    const w_seed_accelerated_provider0_native_artifact *artifact,
    w_seed_accelerated_provider0_callback_event *event) {
  (void)request_program;
  (void)request_result;
  (void)artifact;
  gpu0_cuda_context *context = (gpu0_cuda_context *)raw;
  if (context == NULL || event == NULL || context->provider_path == NULL ||
      context->ptx == NULL || context->kernel_name == NULL)
    return false;
  set_event_identity(context, event);
  context->library = LoadLibraryA(context->provider_path);
  if (context->library == NULL) {
    event_cuda_failure(event, (CUresult)GetLastError());
    return true;
  }
  context->effect_started = true;
  event->effect_started = true;
  const char *missing = NULL;
  if (!load_api(context->library, &context->api, &missing)) {
    (void)missing;
    event_cuda_failure(event, 0);
    return true;
  }
  CUresult status = context->api.cuInit(0u);
  if (status != 0) {
    event_cuda_failure(event, status);
    return true;
  }
  status = context->api.cuDeviceGet(&context->device, 0);
  if (status != 0) {
    event_cuda_failure(event, status);
    return true;
  }
  status = context->api.cuCtxCreate_v2(&context->context, 0u,
                                       context->device);
  if (status != 0 || context->context == NULL) {
    event_cuda_failure(event, status);
    return true;
  }
  status = context->api.cuModuleLoadData(&context->module, context->ptx);
  if (status != 0 || context->module == NULL) {
    event_cuda_failure(event, status);
    return true;
  }
  status = context->api.cuModuleGetFunction(&context->function,
                                             context->module,
                                             context->kernel_name);
  if (status != 0 || context->function == NULL) {
    event_cuda_failure(event, status);
    return true;
  }
  status = context->api.cuMemAlloc_v2(&context->device_result,
                                      sizeof(context->host_result));
  if (status != 0 || context->device_result == 0u) {
    event_cuda_failure(event, status);
    return true;
  }
  event->status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_OK;
  return true;
}

static bool cuda_submit(void *raw,
                        w_seed_accelerated_provider0_callback_event *event) {
  gpu0_cuda_context *context = (gpu0_cuda_context *)raw;
  if (context == NULL || event == NULL || context->context == NULL ||
      context->function == NULL || context->device_result == 0u)
    return false;
  set_event_identity(context, event);
  context->effect_started = true;
  event->effect_started = true;
  context->host_result = 0;
  CUresult status = context->api.cuMemcpyHtoD_v2(
      context->device_result, &context->host_result,
      sizeof(context->host_result));
  if (status != 0) {
    event_cuda_failure(event, status);
    return true;
  }
  void *kernel_parameters[1] = {&context->device_result};
  status = context->api.cuLaunchKernel(context->function, 1u, 1u, 1u, 1u,
                                       1u, 1u, 0u, NULL, kernel_parameters,
                                       NULL);
  if (status != 0) {
    event_cuda_failure(event, status);
    return true;
  }
  event->status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_OK;
  return true;
}

static bool cuda_join(void *raw,
                      w_seed_accelerated_provider0_callback_event *event) {
  gpu0_cuda_context *context = (gpu0_cuda_context *)raw;
  if (context == NULL || event == NULL || context->context == NULL ||
      context->function == NULL || context->device_result == 0u)
    return false;
  set_event_identity(context, event);
  context->effect_started = true;
  event->effect_started = true;
  CUresult status = context->api.cuCtxSynchronize();
  if (status != 0) {
    event_cuda_failure(event, status);
    return true;
  }
  status = context->api.cuMemcpyDtoH_v2(&context->host_result,
                                         context->device_result,
                                         sizeof(context->host_result));
  if (status != 0) {
    event_cuda_failure(event, status);
    return true;
  }
  event->result_value = context->host_result;
  event->body_settled = true;
  event->provider_drained = true;
  event->status = W_SEED_ACCELERATED_PROVIDER0_CALLBACK_OK;
  return true;
}

static bool cuda_cleanup(
    void *raw, w_seed_accelerated_provider0_callback_event *event) {
  gpu0_cuda_context *context = (gpu0_cuda_context *)raw;
  if (context == NULL || event == NULL) return false;
  set_event_identity(context, event);
  event->effect_started = context->effect_started;
  bool okay = true;
  CUresult first_status = 0;
  if (context->device_result != 0u) {
    if (context->api.cuMemFree_v2 == NULL) {
      okay = false;
    } else {
      const CUresult status = context->api.cuMemFree_v2(context->device_result);
      if (status != 0) {
        okay = false;
        if (first_status == 0) first_status = status;
      }
    }
    context->device_result = 0u;
  }
  if (context->module != NULL) {
    if (context->api.cuModuleUnload == NULL) {
      okay = false;
    } else {
      const CUresult status = context->api.cuModuleUnload(context->module);
      if (status != 0) {
        okay = false;
        if (first_status == 0) first_status = status;
      }
    }
    context->module = NULL;
  }
  if (context->context != NULL) {
    if (context->api.cuCtxDestroy_v2 == NULL) {
      okay = false;
    } else {
      const CUresult status = context->api.cuCtxDestroy_v2(context->context);
      if (status != 0) {
        okay = false;
        if (first_status == 0) first_status = status;
      }
    }
    context->context = NULL;
  }
  if (context->library != NULL) {
    if (!FreeLibrary(context->library)) {
      okay = false;
      if (first_status == 0) first_status = (CUresult)GetLastError();
    }
    context->library = NULL;
  }
  context->function = NULL;
  context->effect_started = false;
  event->cleanup_succeeded = okay;
  if (!okay) event_cuda_failure(event, first_status);
  return true;
}

static bool elapsed_ticks(LARGE_INTEGER start, LARGE_INTEGER end,
                          int64_t *result) {
  if (result == NULL || start.QuadPart < 0 || end.QuadPart < start.QuadPart)
    return false;
  *result = end.QuadPart - start.QuadPart;
  return true;
}

static bool execute_iteration(gpu0_cuda_context *context, int32_t *host_result,
                              int32_t expected_result,
                              gpu0_cuda_timing *timing) {
  if (context == NULL || host_result == NULL || context->function == NULL ||
      context->device_result == 0u)
    return false;
  *host_result = 0;
  LARGE_INTEGER start;
  LARGE_INTEGER after_h2d;
  LARGE_INTEGER after_dispatch;
  LARGE_INTEGER after_d2h;
  if (timing != NULL && !QueryPerformanceCounter(&start)) return false;
  CUresult status = context->api.cuMemcpyHtoD_v2(
      context->device_result, host_result, sizeof(*host_result));
  if (status != 0) {
    report_cuda("cuMemcpyHtoD_v2", status);
    return false;
  }
  if (timing != NULL && !QueryPerformanceCounter(&after_h2d)) return false;
  void *kernel_parameters[1] = {&context->device_result};
  status = context->api.cuLaunchKernel(context->function, 1u, 1u, 1u, 1u,
                                       1u, 1u, 0u, NULL, kernel_parameters,
                                       NULL);
  if (status != 0) {
    report_cuda("cuLaunchKernel", status);
    return false;
  }
  status = context->api.cuCtxSynchronize();
  if (status != 0) {
    report_cuda("cuCtxSynchronize", status);
    return false;
  }
  if (timing != NULL && !QueryPerformanceCounter(&after_dispatch)) return false;
  status = context->api.cuMemcpyDtoH_v2(
      host_result, context->device_result, sizeof(*host_result));
  if (status != 0) {
    report_cuda("cuMemcpyDtoH_v2", status);
    return false;
  }
  if (*host_result != expected_result) {
    (void)fprintf(stderr,
                  "GPU0 CUDA adapter: result verification failed "
                  "(expected %ld, got %ld)\n",
                  (long)expected_result, (long)*host_result);
    return false;
  }
  if (timing == NULL) return true;
  if (!QueryPerformanceCounter(&after_d2h) ||
      !elapsed_ticks(start, after_h2d, &timing->h2d_ticks) ||
      !elapsed_ticks(after_h2d, after_dispatch,
                     &timing->dispatch_sync_ticks) ||
      !elapsed_ticks(after_dispatch, after_d2h, &timing->d2h_ticks) ||
      !elapsed_ticks(start, after_d2h, &timing->end_to_end_ticks))
    return false;
  return true;
}

static bool run_normal(const char *provider_path, const char *kernel_name,
                       int32_t expected_cli, const gpu0_request *request,
                       const uint8_t *ptx, size_t ptx_bytes,
                       const w_seed_accelerated_provider0_artifact_receipt *artifact_receipt) {
  if (provider_path == NULL || kernel_name == NULL || request == NULL ||
      ptx == NULL || ptx_bytes == 0u || artifact_receipt == NULL)
    return false;
  gpu0_cuda_context context = {0};
  context.provider_path = provider_path;
  context.ptx = ptx;
  context.kernel_name = kernel_name;
  context.generation = 1u;
  w_seed_accelerated_provider0_text target = {
      (const char *)request->program.text + request->request.target_offset,
      request->request.target_bytes};
  w_seed_accelerated_provider0_text provider = {
      (const char *)request->program.text +
          request->request.provider_class_offset,
      request->request.provider_class_bytes};
  w_seed_accelerated_provider0_native_artifact artifact = {
      ptx, ptx_bytes, target, provider, artifact_receipt};
  static const w_seed_accelerated_provider0_vtable vtable = {
      cuda_stage, cuda_submit, cuda_join, cuda_cleanup};
  w_seed_accelerated_provider0_authority authority;
  if (!w_seed_accelerated_provider0_authority_open(
          &vtable, &context, sizeof(context),
          (w_seed_accelerated_provider0_text){GPU0_CUDA_IMPLEMENTATION,
                                               sizeof(GPU0_CUDA_IMPLEMENTATION) -
                                                   1u},
          1u, &authority))
    return false;
  const w_seed_accelerated_provider0_input input = {
      &request->program, &request->result, &artifact, &authority};
  w_seed_accelerated_provider0_state state;
  w_seed_accelerated_provider0_outcome outcome;
  w_seed_accelerated_provider0_receipt receipt;
  (void)memset(&state, 0, sizeof(state));
  (void)memset(&outcome, 0, sizeof(outcome));
  (void)memset(&receipt, 0, sizeof(receipt));
  const w_seed_accelerated_provider0_status status =
      w_seed_accelerated_provider0_run(&input, &state, &outcome, &receipt);
  if (status != W_SEED_ACCELERATED_PROVIDER0_OK) {
    report_status("ACCPROV0", (uint32_t)status);
    return false;
  }
  if (!w_seed_accelerated_provider0_verify_outcome(&input, &outcome) ||
      !w_seed_accelerated_provider0_verify_receipt(&input, &receipt)) {
    report_message("ACCPROV0 receipt or semantic outcome verification failed");
    return false;
  }
  if (outcome.value != expected_cli) {
    (void)fprintf(stderr,
                  "GPU0 CUDA adapter: result verification failed "
                  "(expected %ld, got %ld)\n",
                  (long)expected_cli, (long)outcome.value);
    return false;
  }
  (void)fprintf(stdout, "GPU0 CUDA result: %ld\n", (long)outcome.value);
  return true;
}

static bool run_benchmark(
    const char *provider_path, const char *kernel_name, int32_t expected,
    const gpu0_request *request, const uint8_t *ptx, size_t ptx_bytes,
    const w_seed_accelerated_provider0_artifact_receipt *artifact_receipt,
    unsigned int warmups, unsigned int samples) {
  gpu0_cuda_context context = {0};
  context.provider_path = provider_path;
  context.ptx = ptx;
  context.kernel_name = kernel_name;
  context.generation = 1u;
  w_seed_accelerated_provider0_text target = {
      (const char *)request->program.text + request->request.target_offset,
      request->request.target_bytes};
  w_seed_accelerated_provider0_text provider = {
      (const char *)request->program.text +
          request->request.provider_class_offset,
      request->request.provider_class_bytes};
  w_seed_accelerated_provider0_native_artifact artifact = {
      ptx, ptx_bytes, target, provider, artifact_receipt};
  w_seed_accelerated_provider0_callback_event stage_event;
  (void)memset(&stage_event, 0, sizeof(stage_event));
  if (!cuda_stage(&context, &request->program, &request->result, &artifact,
                  &stage_event) ||
      stage_event.status != W_SEED_ACCELERATED_PROVIDER0_CALLBACK_OK) {
    report_status("CUDA benchmark stage", stage_event.raw_status);
    (void)cuda_cleanup(&context, &stage_event);
    return false;
  }
  gpu0_cuda_timing *timings =
      (gpu0_cuda_timing *)calloc(samples, sizeof(*timings));
  if (timings == NULL) {
    report_message("benchmark sample allocation failed");
    (void)cuda_cleanup(&context, &stage_event);
    return false;
  }
  bool okay = true;
  int32_t host_result = 0;
  for (unsigned int index = 0u; index < warmups; index += 1u)
    if (!execute_iteration(&context, &host_result, expected, NULL)) {
      okay = false;
      break;
    }
  for (unsigned int index = 0u; okay && index < samples; index += 1u)
    if (!execute_iteration(&context, &host_result, expected, &timings[index]))
      okay = false;
  w_seed_accelerated_provider0_callback_event cleanup_event;
  (void)memset(&cleanup_event, 0, sizeof(cleanup_event));
  if (!cuda_cleanup(&context, &cleanup_event) ||
      !cleanup_event.cleanup_succeeded)
    okay = false;
  if (okay) {
    LARGE_INTEGER frequency;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
      okay = false;
    else {
      (void)fprintf(stdout,
                    "{\"schema\":\"w-gpu0-cuda-timing-1\",\"frequency\":%lld,\"warmups\":%u,\"samples\":[",
                    (long long)frequency.QuadPart, warmups);
      for (unsigned int index = 0u; index < samples; index += 1u) {
        if (index != 0u) (void)fputc(',', stdout);
        (void)fprintf(stdout,
                      "{\"result\":%ld,\"h2d\":%lld,\"dispatchSync\":%lld,\"d2h\":%lld,\"endToEnd\":%lld}",
                      (long)expected, (long long)timings[index].h2d_ticks,
                      (long long)timings[index].dispatch_sync_ticks,
                      (long long)timings[index].d2h_ticks,
                      (long long)timings[index].end_to_end_ticks);
      }
      (void)fprintf(stdout, "],\"result\":%ld}\n", (long)expected);
    }
  }
  free(timings);
  return okay;
}

int main(int argc, char **argv) {
  const bool benchmark = argc == 10 && argv != NULL && argv[5] != NULL &&
                         strcmp(argv[5], "--benchmark") == 0;
  const bool normal = argc == 7;
  unsigned int warmup_count = 0u;
  unsigned int sample_count = 0u;
  int32_t expected_cli = 0;
  if ((!normal && !benchmark) || argv == NULL || argv[1] == NULL ||
      argv[2] == NULL || argv[3] == NULL || argv[4] == NULL ||
      argv[normal ? 5 : 8] == NULL || argv[normal ? 6 : 9] == NULL ||
      argv[1][0] == '\0' || argv[2][0] == '\0' || argv[3][0] == '\0' ||
      !parse_expected_i32(argv[4], &expected_cli)) {
    report_message("usage: gpu0_cuda_windows.exe <provider.dll> <kernel.ptx> "
                   "<kernel-name> <expected-i32> <device.mlir> "
                   "<request.json> [--benchmark <warmups> <odd-samples>]");
    return GPU0_CUDA_FAILURE;
  }
  if (benchmark) {
    size_t parsed_warmups = 0u;
    size_t parsed_samples = 0u;
    if (!parse_size(argv[6], &parsed_warmups) ||
        !parse_size(argv[7], &parsed_samples) || parsed_warmups == 0u ||
        parsed_samples == 0u || parsed_warmups > GPU0_CUDA_MAX_BENCHMARK_ITERATIONS ||
        parsed_samples > GPU0_CUDA_MAX_BENCHMARK_ITERATIONS ||
        (parsed_samples % 2u) == 0u || parsed_warmups > UINT_MAX ||
        parsed_samples > UINT_MAX) {
      report_message("benchmark warmups/samples are outside the bounded contract");
      return GPU0_CUDA_FAILURE;
    }
    warmup_count = (unsigned int)parsed_warmups;
    sample_count = (unsigned int)parsed_samples;
  }

  const char *device_path = argv[normal ? 5 : 8];
  const char *metadata_path = argv[normal ? 6 : 9];
  uint8_t *ptx = NULL;
  uint8_t *device_artifact = NULL;
  uint8_t *metadata_bytes = NULL;
  size_t ptx_bytes = 0u;
  size_t device_artifact_bytes = 0u;
  size_t metadata_byte_count = 0u;
  gpu0_request_metadata metadata;
  gpu0_request request;
  (void)memset(&metadata, 0, sizeof(metadata));
  (void)memset(&request, 0, sizeof(request));
  const bool materialized =
      read_binary(argv[2], GPU0_CUDA_MAX_PTX_BYTES, &ptx, &ptx_bytes) &&
      read_binary(device_path, W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY,
                  &device_artifact, &device_artifact_bytes) &&
      read_binary(metadata_path, GPU0_CUDA_MAX_METADATA_BYTES,
                  &metadata_bytes, &metadata_byte_count) &&
      parse_metadata(metadata_bytes, metadata_byte_count, &metadata) &&
      strcmp(argv[3], metadata.kernel) == 0 &&
      make_request(device_artifact, device_artifact_bytes, &metadata,
                   &request);
  if (!materialized) {
    report_message("verified ACCREQ0 materialization or metadata is invalid");
    free(metadata_bytes);
    free(ptx);
    free(device_artifact);
    return GPU0_CUDA_FAILURE;
  }
  w_seed_accelerated_provider0_artifact_receipt artifact_receipt;
  if (!make_artifact_receipt(&request.program, ptx, ptx_bytes,
                             &artifact_receipt)) {
    report_message("native artifact receipt construction failed");
    free(metadata_bytes);
    free(ptx);
    free(device_artifact);
    return GPU0_CUDA_FAILURE;
  }
  bool ran = false;
  if (benchmark)
    ran = run_benchmark(argv[1], argv[3], expected_cli, &request, ptx,
                        ptx_bytes, &artifact_receipt, warmup_count,
                        sample_count);
  else
    ran = run_normal(argv[1], argv[3], expected_cli, &request, ptx, ptx_bytes,
                     &artifact_receipt);
  free(metadata_bytes);
  free(ptx);
  free(device_artifact);
  return ran ? GPU0_CUDA_SUCCESS : GPU0_CUDA_FAILURE;
}

#else

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  (void)fprintf(stderr, "GPU0 CUDA adapter: native Windows only\n");
  return 2;
}

#endif
