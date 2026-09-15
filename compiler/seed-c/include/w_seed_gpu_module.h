#ifndef W_SEED_GPU_MODULE_H
#define W_SEED_GPU_MODULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_frontend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This bridge is an internal, target-neutral compiler record. It does not
 * select a provider, carry a target, or expose a launch/runtime ABI. */
#define W_SEED_GPU_MODULE_SCHEMA_VERSION "w-seed-gpu-module-1"
#define W_SEED_GPU_MODULE_SHA256_BYTES 32u
#define W_SEED_GPU_MODULE_NONE UINT32_MAX

typedef enum {
  W_SEED_GPU_MODULE_OK = 0,
  W_SEED_GPU_MODULE_INVALID_ARGUMENT,
  W_SEED_GPU_MODULE_INVALID_SCHEMA,
  W_SEED_GPU_MODULE_UNSUPPORTED,
  W_SEED_GPU_MODULE_RANGE,
  W_SEED_GPU_MODULE_ALIAS,
  W_SEED_GPU_MODULE_CAPACITY,
  W_SEED_GPU_MODULE_INCONSISTENT,
} w_seed_gpu_module_status;

/* One compiler-owned module identity copied from a Frontend28 record. The
 * text fields are offsets into the bridge output text store. */
typedef struct {
  uint32_t frontend_module_index;
  uint32_t frontend_accelerator_module_index;
  uint32_t frontend_const_declaration_index;
  w_seed_span source_span;
  w_seed_span const_span;
  size_t source_id_offset;
  size_t source_id_bytes;
  size_t module_id_offset;
  size_t module_id_bytes;
  size_t local_module_name_offset;
  size_t local_module_name_bytes;
  size_t const_name_offset;
  size_t const_name_bytes;
  size_t first_kernel;
  size_t kernel_count;
} w_seed_gpu_module_record;

/* One ordered kernel binding with normalized scalar body facts. No source or
 * frontend pointer survives this record. */
typedef struct {
  size_t owner_module;
  size_t ordinal;
  uint32_t frontend_accelerator_kernel_index;
  uint32_t frontend_function_index;
  uint32_t frontend_statement_index;
  uint32_t frontend_expression_index;
  uint32_t frontend_return_type_index;
  w_seed_span field_span;
  w_seed_span function_span;
  w_seed_span body_span;
  w_seed_span statement_span;
  w_seed_span expression_span;
  size_t label_offset;
  size_t label_bytes;
  size_t function_name_offset;
  size_t function_name_bytes;
  uint16_t return_bit_width;
  bool return_is_signed;
  int64_t payload;
} w_seed_gpu_module_kernel;

typedef struct {
  const w_seed_frontend_input *frontend_input;
  const w_seed_frontend_output *frontend_output;
  const w_seed_frontend_result *frontend_result;
} w_seed_gpu_module_input;

typedef struct {
  w_seed_gpu_module_record *modules;
  size_t module_capacity;
  w_seed_gpu_module_kernel *kernels;
  size_t kernel_capacity;
  uint8_t *text;
  size_t text_capacity;
  uint8_t *frontend_receipt;
  size_t frontend_receipt_capacity;
} w_seed_gpu_module_output;

typedef struct {
  size_t modules;
  size_t kernels;
  size_t text_bytes;
  size_t frontend_receipt_bytes;
} w_seed_gpu_module_counts;

typedef struct {
  w_seed_gpu_module_status status;
  w_seed_gpu_module_counts required;
  w_seed_gpu_module_counts written;
  char schema[sizeof(W_SEED_GPU_MODULE_SCHEMA_VERSION)];
  char frontend_schema[sizeof(W_SEED_FRONTEND_SCHEMA_VERSION)];
  size_t frontend_receipt_bytes;
  uint8_t frontend_receipt_digest[W_SEED_GPU_MODULE_SHA256_BYTES];
  uint8_t semantic_digest[W_SEED_GPU_MODULE_SHA256_BYTES];
  uint8_t provenance_digest[W_SEED_GPU_MODULE_SHA256_BYTES];
} w_seed_gpu_module_result;

/* A read-only view used by the independent bridge verifier. */
typedef struct {
  const w_seed_gpu_module_record *modules;
  size_t module_count;
  size_t module_capacity;
  const w_seed_gpu_module_kernel *kernels;
  size_t kernel_count;
  size_t kernel_capacity;
  const uint8_t *text;
  size_t text_bytes;
  size_t text_capacity;
  const uint8_t *frontend_receipt;
  size_t frontend_receipt_bytes;
  size_t frontend_receipt_capacity;
} w_seed_gpu_module_program;

/* Measure all bridge storage requirements without writing output storage. */
w_seed_gpu_module_status w_seed_gpu_module_measure(
    const w_seed_gpu_module_input *input, w_seed_gpu_module_counts *counts,
    w_seed_gpu_module_result *result);

/* Validate and copy a Frontend28 module set into caller-owned bridge storage. */
w_seed_gpu_module_status w_seed_gpu_module_run(
    const w_seed_gpu_module_input *input, w_seed_gpu_module_output *output,
    w_seed_gpu_module_result *result);

/* Convert a successful output/result pair into an immutable program view. */
bool w_seed_gpu_module_program_from_output(
    const w_seed_gpu_module_output *output,
    const w_seed_gpu_module_result *result,
    w_seed_gpu_module_program *program);

/* Verify bridge records after the frontend and source owners are gone. */
bool w_seed_gpu_module_verify(const w_seed_gpu_module_program *program,
                              const w_seed_gpu_module_result *result);

#ifdef __cplusplus
}
#endif

#endif
