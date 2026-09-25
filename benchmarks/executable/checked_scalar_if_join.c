// C23 correctness reference for checked_scalar_if_join; not a performance or CRT-free claim.
// Expected output cases (argv => exit; stdout):
// [] => 0; "Joined -1\n"
// ["x"] => 0; "Joined 0\n"
// ["x", "x"] => 2; ""
// ["x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x"] => 1; ""

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static bool choose(int8_t value, int8_t *result) {
    if (value == INT8_C(2)) {
        const int16_t product = (int16_t)value * INT16_C(127);
        if (product < INT8_MIN || product > INT8_MAX) return false;
        *result = (int8_t)product;
        return true;
    }
    const int16_t difference = (int16_t)value - INT16_C(1);
    if (difference < INT8_MIN || difference > INT8_MAX) return false;
    *result = (int8_t)difference;
    return true;
}

int main(int argc, char **argv) {
    (void)argv;
    if (argc < 1) return 1;

    const unsigned int user_argument_count = (unsigned int)(argc - 1);
    if (user_argument_count > (unsigned int)INT8_MAX) return 1;

    int8_t result = 0;
    if (!choose((int8_t)user_argument_count, &result)) return 2;

#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif

    if (printf("Joined %d\n", (int)result) < 0) return 1;
    return 0;
}
