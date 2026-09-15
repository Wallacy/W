#ifndef W_SEED_ACCELERATED_INVOCATION0_H
#define W_SEED_ACCELERATED_INVOCATION0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_frontend.h"
#include "w_seed_gpu_module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ACCINV0 is a caller-owned, target/provider-neutral relation for one
 * statically placed accelerated invocation. It is not a HIR record, a queue,
 * a provider handle, a launch ABI, or a device-memory plan. */
#define W_SEED_ACCELERATED_INVOCATION0_SCHEMA_VERSION \
  "w-seed-accelerated-invocation0-1"
#define W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES 32u
#define W_SEED_ACCELERATED_INVOCATION0_MAX_INVOCATIONS 1u
#define W_SEED_ACCELERATED_INVOCATION0_NONE UINT32_MAX

typedef enum {
  W_SEED_ACCELERATED_INVOCATION0_OK = 0,
  W_SEED_ACCELERATED_INVOCATION0_INVALID_ARGUMENT,
  W_SEED_ACCELERATED_INVOCATION0_INVALID_SCHEMA,
  W_SEED_ACCELERATED_INVOCATION0_UNSUPPORTED,
  W_SEED_ACCELERATED_INVOCATION0_RANGE,
  W_SEED_ACCELERATED_INVOCATION0_ALIAS,
  W_SEED_ACCELERATED_INVOCATION0_CAPACITY,
  W_SEED_ACCELERATED_INVOCATION0_INCONSISTENT,
} w_seed_accelerated_invocation0_status;

typedef struct {
  const w_seed_frontend_input *frontend_input;
  const w_seed_frontend_output *frontend_output;
  const w_seed_frontend_result *frontend_result;
  const w_seed_gpu_module_program *gpu_module_program;
  const w_seed_gpu_module_result *gpu_module_result;
} w_seed_accelerated_invocation0_input;

/* One exact `spawn<accelerated-domain> descriptor.field()` relation followed
 * by its lexical `await` join. ACCINV0 v1 deliberately accepts only the
 * zero-parameter bridge proven by gpu-module-1. Runtime argument records and
 * residency are deferred to a later schema. Identity text is copied to the
 * projection text store; all source indices/spans are provenance only. */
typedef struct {
  uint32_t frontend_module_index;
  uint32_t frontend_owner_function_index;
  uint32_t frontend_accelerator_module_index;
  uint32_t frontend_accelerator_kernel_index;
  uint32_t gpu_module_index;
  uint32_t gpu_kernel_index;
  uint32_t source_launch_expression;
  uint32_t source_call_expression;
  uint32_t source_await_expression;
  uint32_t source_launch_binding_statement;
  uint32_t source_result_binding_statement;
  uint32_t result_type_index;
  uint32_t kernel_return_type_index;
  uint32_t domain_index;
  w_seed_frontend_domain_kind domain_kind;
  w_seed_frontend_domain_mode submission;
  uint32_t domain_capabilities;
  uint32_t domain_maximum;
  uint16_t result_bit_width;
  bool result_is_signed;
  size_t domain_name_offset;
  size_t domain_name_bytes;
  size_t module_name_offset;
  size_t module_name_bytes;
  size_t kernel_label_offset;
  size_t kernel_label_bytes;
  size_t function_name_offset;
  size_t function_name_bytes;
  w_seed_span launch_span;
  w_seed_span call_span;
  w_seed_span await_span;
  w_seed_span launch_binding_span;
  w_seed_span result_binding_span;
} w_seed_accelerated_invocation0_record;

typedef struct {
  size_t invocations;
  size_t text_bytes;
} w_seed_accelerated_invocation0_counts;

typedef struct {
  w_seed_accelerated_invocation0_record *invocations;
  size_t invocation_capacity;
  uint8_t *text;
  size_t text_capacity;
} w_seed_accelerated_invocation0_output;

typedef struct {
  const w_seed_accelerated_invocation0_record *invocations;
  size_t invocation_count;
  size_t invocation_capacity;
  const uint8_t *text;
  size_t text_bytes;
  size_t text_capacity;
  uint32_t frontend_module_count;
  uint32_t frontend_function_count;
  uint32_t frontend_statement_count;
  uint32_t frontend_expression_count;
  uint32_t frontend_type_count;
  uint32_t frontend_domain_count;
  uint32_t frontend_accelerator_module_count;
  uint32_t frontend_accelerator_kernel_count;
  uint32_t gpu_module_count;
  uint32_t gpu_kernel_count;
  uint8_t frontend_receipt_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  uint8_t gpu_semantic_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  uint8_t gpu_provenance_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
} w_seed_accelerated_invocation0_program;

typedef struct {
  w_seed_accelerated_invocation0_status status;
  w_seed_accelerated_invocation0_counts required;
  w_seed_accelerated_invocation0_counts written;
  char schema[sizeof(W_SEED_ACCELERATED_INVOCATION0_SCHEMA_VERSION)];
  char frontend_schema[sizeof(W_SEED_FRONTEND_SCHEMA_VERSION)];
  char gpu_module_schema[sizeof(W_SEED_GPU_MODULE_SCHEMA_VERSION)];
  uint32_t frontend_module_count;
  uint32_t frontend_function_count;
  uint32_t frontend_statement_count;
  uint32_t frontend_expression_count;
  uint32_t frontend_type_count;
  uint32_t frontend_domain_count;
  uint32_t frontend_accelerator_module_count;
  uint32_t frontend_accelerator_kernel_count;
  uint32_t gpu_module_count;
  uint32_t gpu_kernel_count;
  uint8_t frontend_receipt_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  uint8_t gpu_semantic_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  uint8_t gpu_provenance_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  uint8_t semantic_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
  uint8_t provenance_digest[W_SEED_ACCELERATED_INVOCATION0_SHA256_BYTES];
} w_seed_accelerated_invocation0_result;

/* Measure and emit are transactional. A failure leaves every destination
 * byte, including the result, unchanged. */
w_seed_accelerated_invocation0_status w_seed_accelerated_invocation0_measure(
    const w_seed_accelerated_invocation0_input *input,
    w_seed_accelerated_invocation0_counts *counts,
    w_seed_accelerated_invocation0_result *result);

w_seed_accelerated_invocation0_status w_seed_accelerated_invocation0_run(
    const w_seed_accelerated_invocation0_input *input,
    const w_seed_accelerated_invocation0_output *output,
    w_seed_accelerated_invocation0_result *result);

/* Convert a successful caller-owned output/result pair into a source-free
 * program view. The view remains valid after frontend/gpu-module owners are
 * released, provided the output storage remains alive. */
bool w_seed_accelerated_invocation0_program_from_output(
    const w_seed_accelerated_invocation0_output *output,
    const w_seed_accelerated_invocation0_result *result,
    w_seed_accelerated_invocation0_program *program);

/* Re-derive the fixed relation, copied identities, and both digests without
 * reading Frontend, source, or gpu-module storage. */
bool w_seed_accelerated_invocation0_verify(
    const w_seed_accelerated_invocation0_program *program,
    const w_seed_accelerated_invocation0_result *result);

#ifdef __cplusplus
}
#endif

#endif
