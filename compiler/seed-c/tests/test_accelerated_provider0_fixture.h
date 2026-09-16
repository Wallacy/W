#ifndef W_SEED_TEST_ACCELERATED_PROVIDER0_FIXTURE_H
#define W_SEED_TEST_ACCELERATED_PROVIDER0_FIXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "w_seed_accelerated_provider0.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This fixture owns the frontend/GPU0/invocation/binding producer storage and
 * returns an ACCREQ0 whose storage remains alive after those producers are
 * cleared. It is test-only and deliberately exposes no production helper. */
bool w_seed_test_accelerated_provider0_make_request(
    w_seed_accelerated_request0_program *request_program,
    w_seed_accelerated_request0_result *request_result);

bool w_seed_test_accelerated_provider0_make_native_receipt(
    const w_seed_accelerated_request0_program *request_program,
    const uint8_t *native_bytes, size_t native_byte_count,
    w_seed_accelerated_provider0_artifact_receipt *receipt);

#ifdef __cplusplus
}
#endif

#endif
