#ifndef W_SEED_ACCELERATED_BINDING0_H
#define W_SEED_ACCELERATED_BINDING0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_accelerated_invocation0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ACCBIND0 is the one-record, caller-owned binding between an independently
 * verified ACCINV0 relation and a closed product/profile selection. It does
 * not own a provider handle, queue, device, source, or launch. */
#define W_SEED_ACCELERATED_BINDING0_SCHEMA_VERSION \
  "w-seed-accelerated-binding0-1"
#define W_SEED_ACCELERATED_BINDING0_PROFILE_SCHEMA_VERSION \
  "w-seed-accelerated-profile0-1"
#define W_SEED_ACCELERATED_BINDING0_SHA256_BYTES 32u
#define W_SEED_ACCELERATED_BINDING0_MAX_RELATIONS 1u

typedef enum {
  W_SEED_ACCELERATED_BINDING0_OK = 0,
  W_SEED_ACCELERATED_BINDING0_INVALID_ARGUMENT,
  W_SEED_ACCELERATED_BINDING0_INVALID_SCHEMA,
  W_SEED_ACCELERATED_BINDING0_ACCINV0_INVALID,
  W_SEED_ACCELERATED_BINDING0_PROFILE_NOT_CLOSED,
  W_SEED_ACCELERATED_BINDING0_MISMATCH,
  W_SEED_ACCELERATED_BINDING0_UNSUPPORTED,
  W_SEED_ACCELERATED_BINDING0_RANGE,
  W_SEED_ACCELERATED_BINDING0_ALIAS,
  W_SEED_ACCELERATED_BINDING0_CAPACITY,
  W_SEED_ACCELERATED_BINDING0_INCONSISTENT,
} w_seed_accelerated_binding0_status;

typedef struct {
  const char *data;
  size_t bytes;
} w_seed_accelerated_binding0_text;

typedef struct {
  uint64_t maximum_in_flight;
  uint64_t maximum_command_bytes;
  uint64_t maximum_argument_bytes;
  uint64_t maximum_result_bytes;
  uint64_t maximum_dependency_edges;
  uint64_t maximum_retained_device_bytes;
  uint64_t maximum_completion_records;
  uint64_t maximum_cleanup_steps;
} w_seed_accelerated_binding0_limits;

typedef enum {
  W_SEED_ACCELERATED_BINDING0_NUMERIC_STRICT = 0,
  W_SEED_ACCELERATED_BINDING0_NUMERIC_REPRODUCIBLE,
  W_SEED_ACCELERATED_BINDING0_NUMERIC_FAST,
} w_seed_accelerated_binding0_numeric_mode;

/* ACCBIND0 is deliberately reject-only. A compatible-domain fallback needs a
 * second closed relation and is not smuggled into this seed cut. */
typedef enum {
  W_SEED_ACCELERATED_BINDING0_FALLBACK_REJECT = 0,
  W_SEED_ACCELERATED_BINDING0_FALLBACK_COMPATIBLE,
} w_seed_accelerated_binding0_fallback;

/* This record is supplied by the closed product/profile owner. All text is a
 * borrowed view for the duration of measure/run and is copied to the output
 * relation. Queue/device/generation values are opaque identities, never
 * handles. */
typedef struct {
  const char *schema;
  size_t schema_bytes;

  w_seed_accelerated_binding0_text root_identity;
  w_seed_accelerated_binding0_text domain_identity;
  w_seed_accelerated_binding0_text descriptor_name;
  w_seed_accelerated_binding0_text kernel_label;
  w_seed_accelerated_binding0_text module_identity;
  w_seed_accelerated_binding0_text artifact_identity;
  w_seed_accelerated_binding0_text artifact_module_identity;
  w_seed_accelerated_binding0_text kernel_instance_identity;
  w_seed_accelerated_binding0_text artifact_target;
  w_seed_accelerated_binding0_text device_target;
  w_seed_accelerated_binding0_text provider_class;
  w_seed_accelerated_binding0_text queue_identity;
  w_seed_accelerated_binding0_text device_identity;
  w_seed_accelerated_binding0_text provider_generation;

  uint32_t bound_gpu_module_index;
  uint32_t bound_gpu_kernel_index;

  uint64_t profile_maximum;
  uint64_t root_maximum;
  uint64_t deployment_maximum;
  w_seed_accelerated_binding0_limits limits;

  w_seed_frontend_domain_mode submission;
  w_seed_accelerated_binding0_numeric_mode numeric_mode;
  w_seed_accelerated_binding0_fallback fallback;

  uint8_t artifact_provider_abi_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t provider_abi_digest[W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t artifact_instances_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t profile_semantic_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t profile_provenance_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t root_binding_digest[W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];

  bool artifact_closed;
  bool root_owned;
  bool provider_resolved;
  bool queue_device_match;
  bool selected_instance_member;
} w_seed_accelerated_binding0_closed_profile;

typedef struct {
  const w_seed_accelerated_invocation0_program *invocation_program;
  const w_seed_accelerated_invocation0_result *invocation_result;
  const w_seed_accelerated_binding0_closed_profile *profile;
} w_seed_accelerated_binding0_input;

typedef struct {
  uint32_t accinv_gpu_module_index;
  uint32_t accinv_gpu_kernel_index;
  uint32_t bound_gpu_module_index;
  uint32_t bound_gpu_kernel_index;

  uint64_t required_maximum;
  uint64_t effective_maximum;
  w_seed_accelerated_binding0_limits limits;

  uint16_t result_bit_width;
  bool result_is_signed;

  w_seed_frontend_domain_mode submission;
  w_seed_accelerated_binding0_numeric_mode numeric_mode;
  w_seed_accelerated_binding0_fallback fallback;

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
  size_t artifact_module_identity_offset;
  size_t artifact_module_identity_bytes;
  size_t kernel_label_offset;
  size_t kernel_label_bytes;
  size_t function_name_offset;
  size_t function_name_bytes;
  size_t kernel_instance_offset;
  size_t kernel_instance_bytes;
  size_t artifact_target_offset;
  size_t artifact_target_bytes;
  size_t device_target_offset;
  size_t device_target_bytes;
  size_t provider_class_offset;
  size_t provider_class_bytes;
  size_t queue_offset;
  size_t queue_bytes;
  size_t device_offset;
  size_t device_bytes;
  size_t generation_offset;
  size_t generation_bytes;

  uint8_t accinv_semantic_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t accinv_provenance_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t profile_semantic_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t profile_provenance_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t artifact_provider_abi_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t provider_abi_digest[W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t artifact_instances_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t root_binding_digest[W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
} w_seed_accelerated_binding0_record;

typedef struct {
  size_t relations;
  size_t text_bytes;
} w_seed_accelerated_binding0_counts;

typedef struct {
  w_seed_accelerated_binding0_record *relations;
  size_t relation_capacity;
  uint8_t *text;
  size_t text_capacity;
} w_seed_accelerated_binding0_output;

typedef struct {
  const w_seed_accelerated_binding0_record *relations;
  size_t relation_count;
  size_t relation_capacity;
  const uint8_t *text;
  size_t text_bytes;
  size_t text_capacity;
  uint8_t semantic_digest[W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t provenance_digest[W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
} w_seed_accelerated_binding0_program;

typedef struct {
  w_seed_accelerated_binding0_status status;
  w_seed_accelerated_binding0_counts required;
  w_seed_accelerated_binding0_counts written;
  char schema[sizeof(W_SEED_ACCELERATED_BINDING0_SCHEMA_VERSION)];
  char invocation_schema[
      sizeof(W_SEED_ACCELERATED_INVOCATION0_SCHEMA_VERSION)];
  char profile_schema[
      sizeof(W_SEED_ACCELERATED_BINDING0_PROFILE_SCHEMA_VERSION)];
  uint64_t required_maximum;
  uint64_t effective_maximum;
  uint8_t accinv_semantic_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t accinv_provenance_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t profile_semantic_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t profile_provenance_digest[
      W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t semantic_digest[W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
  uint8_t provenance_digest[W_SEED_ACCELERATED_BINDING0_SHA256_BYTES];
} w_seed_accelerated_binding0_result;

/* Measure and emit are transactional: failures leave every destination byte
 * unchanged. */
w_seed_accelerated_binding0_status w_seed_accelerated_binding0_measure(
    const w_seed_accelerated_binding0_input *input,
    w_seed_accelerated_binding0_counts *counts,
    w_seed_accelerated_binding0_result *result);

w_seed_accelerated_binding0_status w_seed_accelerated_binding0_run(
    const w_seed_accelerated_binding0_input *input,
    const w_seed_accelerated_binding0_output *output,
    w_seed_accelerated_binding0_result *result);

bool w_seed_accelerated_binding0_program_from_output(
    const w_seed_accelerated_binding0_output *output,
    const w_seed_accelerated_binding0_result *result,
    w_seed_accelerated_binding0_program *program);

/* Re-derive the relation and both digests without reading ACCINV0/profile
 * producer storage. */
bool w_seed_accelerated_binding0_verify(
    const w_seed_accelerated_binding0_program *program,
    const w_seed_accelerated_binding0_result *result);

#ifdef __cplusplus
}
#endif

#endif
