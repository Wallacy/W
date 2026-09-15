#ifndef W_SEED_GPU0_PROJECTION_H
#define W_SEED_GPU0_PROJECTION_H

#include <stdbool.h>
#include <stddef.h>

#include "w_seed_gpu0.h"
#include "w_seed_gpu_module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The projection copies the three source identities into this caller-owned
 * text store. The returned GPU0 program therefore remains valid after the
 * verified gpu-module program and its source/frontend owners are released. */
typedef struct {
  w_seed_gpu0_function *functions;
  size_t function_capacity;
  w_seed_gpu0_operation *operations;
  size_t operation_capacity;
  char *text;
  size_t text_capacity;
} w_seed_gpu0_projection_output;

/* Project one verified kernel binding into the exact GPU0 shape. The module
 * index selects a compiler-owned module and kernel ordinal selects its binding.
 * Destination records and identity text remain caller-owned; no heap storage
 * or provider/runtime mechanism is used. */
bool w_seed_gpu0_program_from_gpu_module(
    const w_seed_gpu_module_program *module_program,
    const w_seed_gpu_module_result *module_result, size_t module_index,
    size_t kernel_ordinal, const w_seed_gpu0_projection_output *output,
    w_seed_gpu0_program *program);

#ifdef __cplusplus
}
#endif

#endif
