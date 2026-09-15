#include "w_seed_parallel_local_provider1.h"

#include "w_seed_sha256.h"

#include <string.h>

static const uint8_t local_provider1_seal = 0x57u;

static void sha_u32(w_seed_sha256_state *state, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)(value >> 24u),
                            (uint8_t)(value >> 16u),
                            (uint8_t)(value >> 8u), (uint8_t)value};
  w_seed_sha256_update(state, bytes, sizeof(bytes));
}

static void canonical_receipt(
    w_seed_parallel_local_provider1_receipt *receipt) {
  (void)memset(receipt, 0, sizeof(*receipt));
  (void)memcpy(receipt->schema,
               W_SEED_PARALLEL_LOCAL_PROVIDER1_SCHEMA_VERSION,
               sizeof(receipt->schema));
  receipt->target = W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_AMD64;
  receipt->generation = 1u;
  (void)memcpy(receipt->domain,
               W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_DOMAIN,
               sizeof(W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_DOMAIN));
  (void)memcpy(receipt->profile,
               W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_PROFILE,
               sizeof(W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_PROFILE));
  (void)memcpy(receipt->identity,
               W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_IDENTITY,
               sizeof(W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_IDENTITY));
  receipt->assurance =
      W_SEED_PARALLEL_LOCAL_PROVIDER1_ASSURANCE_STATIC_LOCAL_BINDING;

  w_seed_sha256_state state;
  w_seed_sha256_init(&state);
  w_seed_sha256_update(
      &state,
      (const uint8_t *)W_SEED_PARALLEL_LOCAL_PROVIDER1_SCHEMA_VERSION,
      sizeof(W_SEED_PARALLEL_LOCAL_PROVIDER1_SCHEMA_VERSION) - 1u);
  sha_u32(&state, receipt->target);
  sha_u32(&state, receipt->generation);
  w_seed_sha256_update(&state, (const uint8_t *)receipt->domain,
                       sizeof(receipt->domain));
  w_seed_sha256_update(&state, (const uint8_t *)receipt->profile,
                       sizeof(receipt->profile));
  w_seed_sha256_update(&state, (const uint8_t *)receipt->identity,
                       sizeof(receipt->identity));
  sha_u32(&state, (uint32_t)receipt->assurance);
  w_seed_sha256_final(&state, receipt->contract_digest);
}

bool w_seed_parallel_local_provider1_open(
    uint32_t target, w_seed_parallel_local_provider1_authority *authority) {
  if (authority == NULL ||
      target != W_SEED_PARALLEL_LOCAL_PROVIDER1_WINDOWS_AMD64)
    return false;
#if defined(_WIN32) && defined(_WIN64)
  w_seed_parallel_local_provider1_authority candidate;
  (void)memset(&candidate, 0, sizeof(candidate));
  candidate.private_seal = (uintptr_t)&local_provider1_seal;
  canonical_receipt(&candidate.receipt);
  *authority = candidate;
  return true;
#else
  return false;
#endif
}

bool w_seed_parallel_local_provider1_verify(
    const w_seed_parallel_local_provider1_authority *authority) {
  if (authority == NULL ||
      authority->private_seal != (uintptr_t)&local_provider1_seal)
    return false;
  w_seed_parallel_local_provider1_receipt expected;
  canonical_receipt(&expected);
  return w_seed_parallel_local_provider1_receipt_equal(&authority->receipt,
                                                        &expected);
}

bool w_seed_parallel_local_provider1_receipt_equal(
    const w_seed_parallel_local_provider1_receipt *left,
    const w_seed_parallel_local_provider1_receipt *right) {
  return left != NULL && right != NULL &&
         memcmp(left->schema, right->schema, sizeof(left->schema)) == 0 &&
         left->target == right->target &&
         left->generation == right->generation &&
         memcmp(left->domain, right->domain, sizeof(left->domain)) == 0 &&
         memcmp(left->profile, right->profile, sizeof(left->profile)) == 0 &&
         memcmp(left->identity, right->identity, sizeof(left->identity)) == 0 &&
         memcmp(left->contract_digest, right->contract_digest,
                sizeof(left->contract_digest)) == 0 &&
         left->assurance == right->assurance;
}

w_seed_parallel_provider0_platform_status
w_seed_parallel_local_provider1_execute(
    const w_seed_parallel_local_provider1_authority *authority,
    const w_seed_parallel_platform1_job *job, size_t job_count,
    uint32_t provider_capacity,
    w_seed_parallel_platform1_completion *completions,
    w_seed_parallel_platform1_receipt *receipt,
    w_seed_parallel_provider0_kind *provider_kind) {
  if (!w_seed_parallel_local_provider1_verify(authority))
    return W_SEED_PARALLEL_PROVIDER0_PLATFORM_PROVIDER_FAILURE;
  return w_seed_parallel_platform1_execute(
      job, job_count, provider_capacity, completions, receipt, provider_kind);
}
