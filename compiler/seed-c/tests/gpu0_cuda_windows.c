/*
 * GPU0's deliberately small Windows driver adapter.
 *
 * This file does not include CUDA headers and does not link an import
 * library.  The CUDA Driver ABI declarations below are the only provider
 * surface used by this experimental probe.  It is not the W runtime.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>

enum {
  GPU0_CUDA_FAILURE = 2,
  GPU0_CUDA_SUCCESS = 0,
  GPU0_CUDA_MAX_PTX_BYTES = 16 * 1024 * 1024,
  GPU0_CUDA_MAX_BENCHMARK_ITERATIONS = 10001,
};

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

static void report_message(const char *message) {
  if (message != NULL)
    (void)fprintf(stderr, "GPU0 CUDA adapter: %s\n", message);
}

static void report_cuda(const char *operation, CUresult status) {
  (void)fprintf(stderr, "GPU0 CUDA adapter: %s failed (CUDA status %d)\n",
                operation, status);
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

static bool read_ptx(const char *path, uint8_t **bytes_out, size_t *size_out) {
  if (path == NULL || path[0] == '\0' || bytes_out == NULL ||
      size_out == NULL)
    return false;
  *bytes_out = NULL;
  *size_out = 0u;

  FILE *file = NULL;
  if (fopen_s(&file, path, "rb") != 0 || file == NULL) return false;
  bool okay = _fseeki64(file, 0, SEEK_END) == 0;
  const int64_t signed_size = okay ? (int64_t)_ftelli64(file) : -1;
  okay = okay && signed_size > 0 &&
         signed_size <= (int64_t)GPU0_CUDA_MAX_PTX_BYTES &&
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

static bool parse_iteration_count(const char *text, unsigned int *value) {
  if (text == NULL || value == NULL || text[0] == '\0') return false;
  char *end = NULL;
  const unsigned long parsed = strtoul(text, &end, 10);
  if (end == text || end == NULL || *end != '\0' || parsed == 0ul ||
      parsed > (unsigned long)GPU0_CUDA_MAX_BENCHMARK_ITERATIONS)
    return false;
  *value = (unsigned int)parsed;
  return true;
}

static bool elapsed_ticks(LARGE_INTEGER start, LARGE_INTEGER end,
                          int64_t *result) {
  if (result == NULL || start.QuadPart < 0 || end.QuadPart < start.QuadPart)
    return false;
  *result = end.QuadPart - start.QuadPart;
  return true;
}

static bool execute_iteration(const gpu0_cuda_api *api, CUfunction function,
                              CUdeviceptr device_result, int32_t *host_result,
                              gpu0_cuda_timing *timing) {
  if (api == NULL || function == NULL || device_result == 0u ||
      host_result == NULL)
    return false;
  *host_result = 0;
  LARGE_INTEGER start;
  LARGE_INTEGER after_h2d;
  LARGE_INTEGER after_dispatch;
  LARGE_INTEGER after_d2h;
  if (timing != NULL && !QueryPerformanceCounter(&start)) return false;

  CUresult status =
      api->cuMemcpyHtoD_v2(device_result, host_result, sizeof(*host_result));
  if (status != GPU0_CUDA_SUCCESS) {
    report_cuda("cuMemcpyHtoD_v2", status);
    return false;
  }
  if (timing != NULL && !QueryPerformanceCounter(&after_h2d)) return false;

  void *kernel_parameters[1] = {&device_result};
  status = api->cuLaunchKernel(function, 1u, 1u, 1u, 1u, 1u, 1u, 0u, NULL,
                               kernel_parameters, NULL);
  if (status != GPU0_CUDA_SUCCESS) {
    report_cuda("cuLaunchKernel", status);
    return false;
  }
  status = api->cuCtxSynchronize();
  if (status != GPU0_CUDA_SUCCESS) {
    report_cuda("cuCtxSynchronize", status);
    return false;
  }
  if (timing != NULL && !QueryPerformanceCounter(&after_dispatch)) return false;

  status = api->cuMemcpyDtoH_v2(host_result, device_result,
                                sizeof(*host_result));
  if (status != GPU0_CUDA_SUCCESS) {
    report_cuda("cuMemcpyDtoH_v2", status);
    return false;
  }
  if (*host_result != INT32_C(42)) {
    (void)fprintf(stderr,
                  "GPU0 CUDA adapter: result verification failed (got %ld)\n",
                  (long)*host_result);
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

int main(int argc, char **argv) {
  const bool benchmark = argc == 7 && argv != NULL && argv[4] != NULL &&
                         strcmp(argv[4], "--benchmark") == 0;
  unsigned int warmup_count = 0u;
  unsigned int sample_count = 0u;
  if ((argc != 4 && !benchmark) || argv == NULL || argv[1] == NULL || argv[2] == NULL ||
      argv[3] == NULL || argv[1][0] == '\0' || argv[2][0] == '\0' ||
      argv[3][0] == '\0' ||
      (benchmark && (!parse_iteration_count(argv[5], &warmup_count) ||
                     !parse_iteration_count(argv[6], &sample_count) ||
                     (sample_count % 2u) == 0u))) {
    report_message("usage: gpu0_cuda_windows.exe <provider.dll> <kernel.ptx> <kernel-name> [--benchmark <warmups> <odd-samples>]");
    return GPU0_CUDA_FAILURE;
  }

  HMODULE library = NULL;
  gpu0_cuda_api api;
  (void)memset(&api, 0, sizeof(api));
  CUcontext context = NULL;
  CUmodule module = NULL;
  CUdeviceptr device_result = 0u;
  uint8_t *ptx = NULL;
  size_t ptx_bytes = 0u;
  gpu0_cuda_timing *timings = NULL;
  LARGE_INTEGER performance_frequency = {0};
  int exit_code = GPU0_CUDA_FAILURE;
  bool primary_failure = false;

  if (benchmark &&
      (!QueryPerformanceFrequency(&performance_frequency) ||
       performance_frequency.QuadPart <= 0)) {
    report_message("QueryPerformanceFrequency failed");
    return GPU0_CUDA_FAILURE;
  }

  library = LoadLibraryA(argv[1]);
  if (library == NULL) {
    report_message("provider LoadLibraryA failed");
    primary_failure = true;
    goto cleanup;
  }

  const char *missing_symbol = NULL;
  if (!load_api(library, &api, &missing_symbol)) {
    (void)fprintf(stderr, "GPU0 CUDA adapter: missing provider symbol %s\n",
                  missing_symbol == NULL ? "<unknown>" : missing_symbol);
    primary_failure = true;
    goto cleanup;
  }

  if (!read_ptx(argv[2], &ptx, &ptx_bytes)) {
    report_message("PTX path/read failed or exceeds the bounded input limit");
    primary_failure = true;
    goto cleanup;
  }
  (void)ptx_bytes;

  CUresult status = api.cuInit(0u);
  if (status != GPU0_CUDA_SUCCESS) {
    report_cuda("cuInit", status);
    primary_failure = true;
    goto cleanup;
  }

  CUdevice device = 0;
  status = api.cuDeviceGet(&device, 0);
  if (status != GPU0_CUDA_SUCCESS) {
    report_cuda("cuDeviceGet", status);
    primary_failure = true;
    goto cleanup;
  }

  status = api.cuCtxCreate_v2(&context, 0u, device);
  if (status != GPU0_CUDA_SUCCESS || context == NULL) {
    if (status == GPU0_CUDA_SUCCESS)
      report_message("cuCtxCreate_v2 returned no context");
    else
      report_cuda("cuCtxCreate_v2", status);
    primary_failure = true;
    goto cleanup;
  }

  status = api.cuModuleLoadData(&module, ptx);
  if (status != GPU0_CUDA_SUCCESS || module == NULL) {
    if (status == GPU0_CUDA_SUCCESS)
      report_message("cuModuleLoadData returned no module");
    else
      report_cuda("cuModuleLoadData", status);
    primary_failure = true;
    goto cleanup;
  }

  CUfunction function = NULL;
  status = api.cuModuleGetFunction(&function, module, argv[3]);
  if (status != GPU0_CUDA_SUCCESS || function == NULL) {
    if (status == GPU0_CUDA_SUCCESS)
      report_message("cuModuleGetFunction returned no kernel");
    else
      report_cuda("cuModuleGetFunction", status);
    primary_failure = true;
    goto cleanup;
  }

  int32_t host_result = 0;
  status = api.cuMemAlloc_v2(&device_result, sizeof(host_result));
  if (status != GPU0_CUDA_SUCCESS || device_result == 0u) {
    if (status == GPU0_CUDA_SUCCESS)
      report_message("cuMemAlloc_v2 returned no device allocation");
    else
      report_cuda("cuMemAlloc_v2", status);
    primary_failure = true;
    goto cleanup;
  }

  if (benchmark) {
    timings = (gpu0_cuda_timing *)calloc(sample_count, sizeof(*timings));
    if (timings == NULL) {
      report_message("benchmark sample allocation failed");
      primary_failure = true;
      goto cleanup;
    }
    for (unsigned int index = 0u; index < warmup_count; index += 1u) {
      if (!execute_iteration(&api, function, device_result, &host_result, NULL)) {
        primary_failure = true;
        goto cleanup;
      }
    }
    for (unsigned int index = 0u; index < sample_count; index += 1u) {
      if (!execute_iteration(&api, function, device_result, &host_result,
                             &timings[index])) {
        primary_failure = true;
        goto cleanup;
      }
    }
  } else if (!execute_iteration(&api, function, device_result, &host_result,
                                NULL)) {
    primary_failure = true;
    goto cleanup;
  }
  exit_code = GPU0_CUDA_SUCCESS;

cleanup:
  if (device_result != 0u && api.cuMemFree_v2 != NULL) {
    status = api.cuMemFree_v2(device_result);
    if (status != GPU0_CUDA_SUCCESS) {
      report_cuda("cuMemFree_v2 cleanup", status);
      exit_code = GPU0_CUDA_FAILURE;
    }
    device_result = 0u;
  }
  if (module != NULL && api.cuModuleUnload != NULL) {
    status = api.cuModuleUnload(module);
    if (status != GPU0_CUDA_SUCCESS) {
      report_cuda("cuModuleUnload cleanup", status);
      exit_code = GPU0_CUDA_FAILURE;
    }
    module = NULL;
  }
  if (context != NULL && api.cuCtxDestroy_v2 != NULL) {
    status = api.cuCtxDestroy_v2(context);
    if (status != GPU0_CUDA_SUCCESS) {
      report_cuda("cuCtxDestroy_v2 cleanup", status);
      exit_code = GPU0_CUDA_FAILURE;
    }
    context = NULL;
  }
  free(ptx);
  if (library != NULL && !FreeLibrary(library)) {
    report_message("FreeLibrary cleanup failed");
    exit_code = GPU0_CUDA_FAILURE;
  }
  if (primary_failure) exit_code = GPU0_CUDA_FAILURE;
  if (exit_code == GPU0_CUDA_SUCCESS && benchmark) {
    (void)fprintf(stdout,
                  "{\"schema\":\"w-gpu0-cuda-timing-1\",\"frequency\":%lld,\"warmups\":%u,\"samples\":[",
                  (long long)performance_frequency.QuadPart, warmup_count);
    for (unsigned int index = 0u; index < sample_count; index += 1u) {
      if (index != 0u) (void)fputc(',', stdout);
      (void)fprintf(stdout,
                    "{\"result\":42,\"h2d\":%lld,\"dispatchSync\":%lld,\"d2h\":%lld,\"endToEnd\":%lld}",
                    (long long)timings[index].h2d_ticks,
                    (long long)timings[index].dispatch_sync_ticks,
                    (long long)timings[index].d2h_ticks,
                    (long long)timings[index].end_to_end_ticks);
    }
    (void)fputs("],\"result\":42}\n", stdout);
  }
  free(timings);
  if (exit_code == GPU0_CUDA_SUCCESS && !benchmark)
    (void)fprintf(stdout, "GPU0 CUDA result: 42\n");
  return exit_code;
}

#else

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  (void)fprintf(stderr, "GPU0 CUDA adapter: native Windows only\n");
  return 2;
}

#endif
