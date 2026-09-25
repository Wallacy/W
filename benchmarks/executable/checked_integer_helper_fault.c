// C23 correctness reference for checked_integer_helper_fault; not a performance or CRT-free claim.
// Expected user-argument cases: 0 -> exit 0, stdout "Begin 255\n", empty stderr;
// 1 -> exit 2, empty stdout and stderr; 256 -> exit 1, empty stdout and stderr.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static bool checked_add_u8(uint8_t left, uint8_t right, uint8_t *result) {
    const unsigned int sum = (unsigned int)left + (unsigned int)right;
    if (sum > (unsigned int)UINT8_MAX) return false;
    *result = (uint8_t)sum;
    return true;
}

int main(int argc, char **argv) {
    (void)argv;
    if (argc < 1) return 1;

    const unsigned int user_argument_count = (unsigned int)(argc - 1);
    if (user_argument_count > (unsigned int)UINT8_MAX) return 1;

    const uint8_t value = (uint8_t)user_argument_count;
    uint8_t result;
    if (!checked_add_u8(value, UINT8_MAX, &result)) return 2;

#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif

    if (printf("Begin %u\n", (unsigned int)result) < 0) return 1;
    return 0;
}
