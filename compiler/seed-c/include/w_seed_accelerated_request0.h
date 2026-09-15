#ifndef W_SEED_ACCELERATED_REQUEST0_H
#define W_SEED_ACCELERATED_REQUEST0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_accelerated_binding0.h"
#include "w_seed_gpu0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ACCREQ0 is a provider-neutral, caller-owned request for the current GPU0
 * sentinel. It owns copied identity and device-artifact bytes. It contains no
 * provider handle, pointer, submission receipt, or public ABI. */
#define W_SEED_ACCELERATED_REQUEST0_SCHEMA_VERSION \
  "w-seed-accelerated-request0-1"
#define W_SEED_ACCELERATED_REQUEST0_SHA256_BYTES 32u
#define W_SEED_ACCELERATED_REQUEST0_MAX_REQUESTS 1u

typedef enum {
  W_SEED_ACCELERATED_REQUEST0_OK = 0,
  W_SEED_ACCELERATED_REQUEST0_INVALID_ARGUMENT,
  W_SEED_ACCELERATED_REQUEST0_INVALID_BINDING,
  W_SEED_ACCELERATED_REQUEST0_INVALID_GPU0,
  W_SEED_ACCELERATED_REQUEST0_MISMATCH,
  W_SEED_ACCELERATED_REQUEST0_UNSUPPORTED,
  W_SEED_ACCELERATED_REQUEST0_RANGE,
  W_SEED_ACCELERATED_REQUEST0_ALIAS,
  W_SEED_ACCELERATED_REQUEST0_CAPACITY,
  W_SEED_ACCELERATED_REQUEST0_INCONSISTENT,
} w_seed_accelerated_request0_status;

typedef struct {
  const w_seed_accelerated_binding0_program *binding_program;
  const w_seed_accelerated_binding0_result *binding_result;
  const w_seed_gpu0_program *gpu_program;
  const w_seed_gpu0_output *gpu_output;
  const w_seed_gpu0_result *gpu_result;
} w_seed_accelerated_request0_input;

typedef struct {
  size_t root_offset;
  size_t root_bytes;
  size_t domain_offset;
  size_t domain_bytes;
  size_t descriptor_offset;
  size_t descriptor_bytes;
  size_t module_identity_offset;
  size_t module_identity_bytes;
  size_t artifact_identity_offset;
  size_t artifact_identity_bytes;
  size_t kernel_label_offset;
  size_t kernel_label_bytes;
  size_t function_name_offset;
  size_t function_name_bytes;
  size_t kernel_instance_offset;
  size_t kernel_instance_bytes;
  size_t target_offset;
  size_t target_bytes;
  size_t provider_class_offset;
  size_t provider_class_bytes;
  size_t kernel_symbol_offset;
  size_t kernel_symbol_bytes;
  size_t queue_offset;
  size_t queue_bytes;
  size_t device_offset;
  size_t device_bytes;
  size_t generation_offset;
  size_t generation_bytes;

  size_t device_artifact_bytes;
  uint32_t result_bytes;
  uint16_t result_bit_width;
  bool result_is_signed;
  int32_t sentinel_expected_i32;

  uint8_t binding_semantic_digest[W_SEED_ACCELERATED_REQUEST0_SHA256_BYTES];
  uint8_t binding_provenance_digest[W_SEED_ACCELERATED_REQUEST0_SHA256_BYTES];
  uint8_t gpu_semantic_identity[W_SEED_ACCELERATED_REQUEST0_SHA256_BYTES];
  uint8_t device_artifact_identity_digest
      [W_SEED_ACCELERATED_REQUEST0_SHA256_BYTES];
  uint8_t device_artifact_digest[W_SEED_ACCELERATED_REQUEST0_SHA256_BYTES];
} w_seed_accelerated_request0_record;

typedef struct {
  size_t requests;
  size_t text_bytes;
  size_t device_artifact_bytes;
} w_seed_accelerated_request0_counts;

typedef struct {
  w_seed_accelerated_request0_record *requests;
  size_t request_capacity;
  uint8_t *text;
  size_t text_capacity;
  uint8_t *device_artifact;
  size_t device_artifact_capacity;
} w_seed_accelerated_request0_output;

typedef struct {
  const w_seed_accelerated_request0_record *requests;
  size_t request_count;
  size_t request_capacity;
  const uint8_t *text;
  size_t text_bytes;
  size_t text_capacity;
  const uint8_t *device_artifact;
  size_t device_artifact_bytes;
  size_t device_artifact_capacity;
  uint8_t semantic_digest[W_SEED_ACCELERATED_REQUEST0_SHA256_BYTES];
  uint8_t provenance_digest[W_SEED_ACCELERATED_REQUEST0_SHA256_BYTES];
} w_seed_accelerated_request0_program;

typedef struct {
  w_seed_accelerated_request0_status status;
  w_seed_accelerated_request0_counts required;
  w_seed_accelerated_request0_counts written;
  char schema[sizeof(W_SEED_ACCELERATED_REQUEST0_SCHEMA_VERSION)];
  char binding_schema[sizeof(W_SEED_ACCELERATED_BINDING0_SCHEMA_VERSION)];
  char gpu_schema[sizeof(W_SEED_GPU0_SCHEMA_VERSION)];
  uint8_t semantic_digest[W_SEED_ACCELERATED_REQUEST0_SHA256_BYTES];
  uint8_t provenance_digest[W_SEED_ACCELERATED_REQUEST0_SHA256_BYTES];
} w_seed_accelerated_request0_result;

w_seed_accelerated_request0_status w_seed_accelerated_request0_measure(
    const w_seed_accelerated_request0_input *input,
    w_seed_accelerated_request0_counts *counts,
    w_seed_accelerated_request0_result *result);

w_seed_accelerated_request0_status w_seed_accelerated_request0_run(
    const w_seed_accelerated_request0_input *input,
    const w_seed_accelerated_request0_output *output,
    w_seed_accelerated_request0_result *result);

bool w_seed_accelerated_request0_program_from_output(
    const w_seed_accelerated_request0_output *output,
    const w_seed_accelerated_request0_result *result,
    w_seed_accelerated_request0_program *program);

bool w_seed_accelerated_request0_verify(
    const w_seed_accelerated_request0_program *program,
    const w_seed_accelerated_request0_result *result);

#ifdef __cplusplus
}
#endif

#endif
