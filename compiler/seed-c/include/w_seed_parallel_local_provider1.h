#ifndef W_SEED_PARALLEL_LOCAL_PROVIDER1_H
#define W_SEED_PARALLEL_LOCAL_PROVIDER1_H

#include "w_seed_parallel_platform1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A process-local compiler-owned authority for one statically linked provider.
 * It is not serializable, a public capability, a binary signature, or registry
 * attestation. Safe W cannot construct or inspect this private seed token. */
#define W_SEED_PARALLEL_LOCAL_PROVIDER1_SCHEMA_VERSION \
  "w-seed-parallel-local-provider1-1"
#define W_SEED_PARALLEL_LOCAL_PROVIDER1_DOMAIN_BYTES 32u
#define W_SEED_PARALLEL_LOCAL_PROVIDER1_PROFILE_BYTES 64u
#define W_SEED_PARALLEL_LOCAL_PROVIDER1_IDENTITY_BYTES 64u
#define W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_AMD64 1u
#define W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_DOMAIN "cpu.parallel"
#define W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_PROFILE \
  "windows-kernel32-platform1@1"
#define W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_IDENTITY \
  "w-seed-platform1-windows-kernel32"

typedef enum {
  W_SEED_PARALLEL_LOCAL_PROVIDER1_ASSURANCE_NONE = 0,
  W_SEED_PARALLEL_LOCAL_PROVIDER1_ASSURANCE_STATIC_LOCAL_BINDING = 1,
} w_seed_parallel_local_provider1_assurance;

typedef struct {
  char schema[sizeof(W_SEED_PARALLEL_LOCAL_PROVIDER1_SCHEMA_VERSION)];
  uint32_t target;
  uint32_t generation;
  char domain[W_SEED_PARALLEL_LOCAL_PROVIDER1_DOMAIN_BYTES];
  char profile[W_SEED_PARALLEL_LOCAL_PROVIDER1_PROFILE_BYTES];
  char identity[W_SEED_PARALLEL_LOCAL_PROVIDER1_IDENTITY_BYTES];
  uint8_t contract_digest[32];
  w_seed_parallel_local_provider1_assurance assurance;
} w_seed_parallel_local_provider1_receipt;

/* `private_seal` is compared only with a private static object in the same
 * process. Its numeric value never enters a digest, receipt, ABI, or output. */
typedef struct {
  uintptr_t private_seal;
  w_seed_parallel_local_provider1_receipt receipt;
} w_seed_parallel_local_provider1_authority;

bool w_seed_parallel_local_provider1_open(
    uint32_t target, w_seed_parallel_local_provider1_authority *authority);

bool w_seed_parallel_local_provider1_verify(
    const w_seed_parallel_local_provider1_authority *authority);

bool w_seed_parallel_local_provider1_receipt_equal(
    const w_seed_parallel_local_provider1_receipt *left,
    const w_seed_parallel_local_provider1_receipt *right);

w_seed_parallel_provider0_platform_status
w_seed_parallel_local_provider1_execute(
    const w_seed_parallel_local_provider1_authority *authority,
    const w_seed_parallel_platform1_job *job, size_t job_count,
    uint32_t provider_capacity,
    w_seed_parallel_platform1_completion *completions,
    w_seed_parallel_platform1_receipt *receipt,
    w_seed_parallel_provider0_kind *provider_kind);

#ifdef __cplusplus
}
#endif

#endif
