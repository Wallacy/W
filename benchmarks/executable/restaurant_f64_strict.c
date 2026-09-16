// C23 reference for the Restaurant strict binary64 arithmetic workload.

#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/*
 * Volatile inputs keep the floating-point operations and NaN comparison in
 * the executable path. The release recipe preserves strict IEEE semantics.
 */
static volatile double sum_left = 1.5;
static volatile double sum_right = 2.25;
static volatile double difference_left = 9.5;
static volatile double difference_right = 5.5;
static volatile double product_left = 1.5;
static volatile double product_right = 2.0;
static volatile double quotient_left = 7.5;
static volatile double quotient_right = 2.5;
static volatile double signed_zero_input = 0.0;
static volatile double nan_left = 0.0;
static volatile double nan_right = 0.0;

static int strict_float_check(void) {
    const double sum = sum_left + sum_right;
    const double difference = difference_left - difference_right;
    const double product = product_left * product_right;
    const double quotient = quotient_left / quotient_right;
    const double signed_zero = -signed_zero_input;
    const double nan = nan_left / nan_right;

    return sum == 3.75 &&
           difference == 4.0 &&
           product == 3.0 &&
           quotient != 4.0 &&
           sum > 3.0 &&
           sum >= 3.75 &&
           quotient < 4.0 &&
           quotient <= 3.0 &&
           signed_zero == 0.0 &&
           nan != nan;
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif

    if (strict_float_check()) {
        if (fputs("Float strict ok\n", stdout) == EOF) return 1;
    } else if (fputs("Float strict bad\n", stdout) == EOF) {
        return 1;
    }
    return 0;
}
