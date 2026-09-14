#ifndef W_SEED_GPU0_H
#define W_SEED_GPU0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GPU0 is a target-neutral, provider-free semantic seed.  It describes the
 * smallest useful host/device split without pretending to allocate device
 * memory or call a driver.  All records and input arrays belong to the
 * caller; this header exposes no runtime handle and the implementation never
 * allocates heap storage.
 */
#define W_SEED_GPU0_SCHEMA_VERSION "w-seed-gpu0-1"
#define W_SEED_GPU0_HOST_ARTIFACT_SCHEMA_VERSION "w-seed-gpu0-host-mlir-1"
#define W_SEED_GPU0_DEVICE_ARTIFACT_SCHEMA_VERSION "w-seed-gpu0-device-mlir-1"
#define W_SEED_GPU0_SHA256_BYTES 32u
#define W_SEED_GPU0_EVIDENCE_FUNCTION_CAPACITY 2u
#define W_SEED_GPU0_EVIDENCE_OPERATION_CAPACITY 7u
#define W_SEED_GPU0_EVIDENCE_RESULT_CAPACITY 64u
#define W_SEED_GPU0_EVIDENCE_ARTIFACT_CAPACITY 8192u
#define W_SEED_GPU0_RESULT_BYTES sizeof(int32_t)
#define W_SEED_GPU0_EXPECTED_PAYLOAD INT32_C(42)
#define W_SEED_GPU0_NONE UINT32_MAX

typedef enum {
  W_SEED_GPU0_OK = 0,
  W_SEED_GPU0_INVALID_ARGUMENT,
  W_SEED_GPU0_INVALID_SCHEMA,
  W_SEED_GPU0_UNSUPPORTED,
  W_SEED_GPU0_RANGE,
  W_SEED_GPU0_ALIAS,
  W_SEED_GPU0_CAPACITY,
  W_SEED_GPU0_INCONSISTENT,
} w_seed_gpu0_status;

typedef enum {
  W_SEED_GPU0_FUNCTION_HOST_ROOT = 0,
  W_SEED_GPU0_FUNCTION_DEVICE_KERNEL = 1,
} w_seed_gpu0_function_role;

/* These bits are semantic facts, not provider capabilities.  GPU0 admits a
 * kernel only when all of them are clear. */
typedef enum {
  W_SEED_GPU0_EFFECT_NONE = 0u,
  W_SEED_GPU0_EFFECT_CAPTURE = 1u << 0,
  W_SEED_GPU0_EFFECT_SUSPENSION = 1u << 1,
  W_SEED_GPU0_EFFECT_RECURSION = 1u << 2,
  W_SEED_GPU0_EFFECT_HOST_IO = 1u << 3,
  W_SEED_GPU0_EFFECT_DYNAMIC_DISPATCH = 1u << 4,
  W_SEED_GPU0_EFFECT_HOST_FFI = 1u << 5,
} w_seed_gpu0_effect;

typedef enum {
  W_SEED_GPU0_ADDRESS_HOST = 0,
  W_SEED_GPU0_ADDRESS_DEVICE = 1,
} w_seed_gpu0_address_space;

/* Offsets are semantic byte ranges in the one bounded host/device result;
 * they are not host pointers and contain no target ABI. */
typedef struct {
  w_seed_gpu0_address_space address_space;
  uint32_t offset;
  uint32_t bytes;
  uint32_t capacity_bytes;
} w_seed_gpu0_range;

typedef enum {
  W_SEED_GPU0_OPERATION_NONE = 0,
  W_SEED_GPU0_OPERATION_ALLOCATE_DEVICE_RESULT,
  W_SEED_GPU0_OPERATION_COPY_HOST_TO_DEVICE,
  W_SEED_GPU0_OPERATION_LAUNCH,
  W_SEED_GPU0_OPERATION_JOIN,
  W_SEED_GPU0_OPERATION_COPY_DEVICE_TO_HOST,
  W_SEED_GPU0_OPERATION_VERIFY_RESULT,
  W_SEED_GPU0_OPERATION_STORE_I32,
} w_seed_gpu0_operation_kind;

typedef struct {
  w_seed_gpu0_operation_kind kind;
  w_seed_gpu0_range source;
  w_seed_gpu0_range destination;
  uint32_t function_index;
  uint32_t dependency_operation;
  int32_t i32_value;
} w_seed_gpu0_operation;

typedef struct {
  const char *name;
  size_t name_length;
  w_seed_gpu0_function_role role;
  uint32_t first_operation;
  uint32_t operation_count;
  uint32_t parameter_count;
  uint32_t effects;
} w_seed_gpu0_function;

/* The program is a bounded semantic IR projection.  It deliberately has no
 * target, provider, queue ordinal, pointer, MLIR dialect handle, or ABI
 * detail.  The exact GPU0 witness has two functions and seven operations. */
typedef struct {
  const w_seed_gpu0_function *functions;
  size_t function_count;
  size_t function_capacity;
  const w_seed_gpu0_operation *operations;
  size_t operation_count;
  size_t operation_capacity;
  uint32_t host_result_capacity_bytes;
  uint32_t device_result_capacity_bytes;
} w_seed_gpu0_program;

typedef enum {
  W_SEED_GPU0_PHASE_STAGED = 0,
  W_SEED_GPU0_PHASE_SUBMITTED,
  W_SEED_GPU0_PHASE_DEVICE_RUNNING,
  W_SEED_GPU0_PHASE_BODY_SETTLED,
  W_SEED_GPU0_PHASE_PROVIDER_DRAINED,
  W_SEED_GPU0_PHASE_CLEANUP,
  W_SEED_GPU0_PHASE_OUTCOME_COMMITTED,
  W_SEED_GPU0_PHASE_JOINED,
} w_seed_gpu0_phase;

typedef struct {
  uint32_t dispatch_count;
  uint32_t join_count;
  uint32_t memory_operation_count;
  uint32_t end_to_end_phase_count;
  size_t device_allocation_bytes;
  size_t host_to_device_bytes;
  size_t device_to_host_bytes;
} w_seed_gpu0_metrics;

typedef struct {
  size_t bytes;
  uint8_t identity_digest[W_SEED_GPU0_SHA256_BYTES];
} w_seed_gpu0_artifact_identity;

typedef struct {
  char schema[sizeof(W_SEED_GPU0_SCHEMA_VERSION)];
  size_t host_artifact_bytes;
  size_t device_artifact_bytes;
  size_t total_artifact_bytes;
  uint8_t semantic_identity[W_SEED_GPU0_SHA256_BYTES];
  w_seed_gpu0_artifact_identity host;
  w_seed_gpu0_artifact_identity device;
  w_seed_gpu0_metrics metrics;
} w_seed_gpu0_measurement;

typedef struct {
  char schema[sizeof(W_SEED_GPU0_SCHEMA_VERSION)];
  size_t bytes;
  uint8_t identity_digest[W_SEED_GPU0_SHA256_BYTES];
  uint8_t artifact_digest[W_SEED_GPU0_SHA256_BYTES];
} w_seed_gpu0_artifact_record;

typedef struct {
  char schema[sizeof(W_SEED_GPU0_SCHEMA_VERSION)];
  w_seed_gpu0_status status;
  w_seed_gpu0_measurement measurement;
  w_seed_gpu0_artifact_record host;
  w_seed_gpu0_artifact_record device;
  uint32_t phase_count;
  w_seed_gpu0_phase phases[8];
  int32_t device_result_value;
  int32_t host_result_value;
} w_seed_gpu0_result;

typedef struct {
  uint8_t *host_artifact;
  size_t host_capacity;
  uint8_t *device_artifact;
  size_t device_capacity;
  /* These two slots are a caller-owned logical execution witness.  GPU0 does
   * not dereference a device pointer or call a provider; a later provider may
   * replace the device slot with real storage without changing the semantic
   * records. */
  int32_t *device_result;
  int32_t *host_result;
} w_seed_gpu0_output;

/* Measure the deterministic host/device semantic records and artifact sizes.
 * On every failure, measurement is left byte-for-byte unchanged. */
w_seed_gpu0_status w_seed_gpu0_measure(
    const w_seed_gpu0_program *program, w_seed_gpu0_measurement *measurement);

/* Emit the two target-neutral MLIR artifacts and complete the provider-free
 * logical witness.  All writes are staged before any caller-owned destination
 * is changed.  This is not real GPU execution. */
w_seed_gpu0_status w_seed_gpu0_run(const w_seed_gpu0_program *program,
                                   const w_seed_gpu0_output *output,
                                   w_seed_gpu0_result *result);

/* Re-derive every semantic fact, artifact byte, identity, digest, phase and
 * logical result without modifying caller-owned storage. */
bool w_seed_gpu0_verify(const w_seed_gpu0_program *program,
                        const w_seed_gpu0_output *output,
                        const w_seed_gpu0_result *result);

#ifdef __cplusplus
}
#endif

#endif
