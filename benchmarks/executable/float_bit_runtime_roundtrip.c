// C23 correctness reference for a runtime-derived f64/u64 bit round trip.
// Expected exit: 0
// Expected stdout:
// Bits 0
// Expected stderr:

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

_Static_assert(sizeof(double) == sizeof(uint64_t),
               "binary64 requires 64-bit double storage");

static uint64_t roundtrip_bits(uint64_t bits) {
    double value;
    uint64_t result;
    memcpy(&value, &bits, sizeof(value));
    memcpy(&result, &value, sizeof(result));
    return result;
}

int main(int argc, char **argv) {
    (void)argv;
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    const uint64_t bits = argc > 0 ? (uint64_t)(argc - 1) : UINT64_C(0);
    if (printf("Bits %" PRIu64 "\n", roundtrip_bits(bits)) < 0) return 1;
    return 0;
}
