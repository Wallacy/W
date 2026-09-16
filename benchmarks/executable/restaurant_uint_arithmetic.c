// C23 reference for the Restaurant checked UInt/u64 arithmetic workload.

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

typedef struct {
    bool ok;
    uint64_t value;
} checked_u64;

/*
 * Volatile operands keep every arithmetic and comparison operation dependent
 * on a runtime load in optimized builds. The values deliberately include the
 * first unsigned value above INT64_MAX and UINT64_MAX.
 */
static volatile uint64_t runtime_high = UINT64_C(9223372036854775808);
static volatile uint64_t runtime_increment = UINT64_C(2);
static volatile uint64_t runtime_one = UINT64_C(1);
static volatile uint64_t runtime_product_left = UINT64_C(3);
static volatile uint64_t runtime_product_right = UINT64_C(7);
static volatile uint64_t runtime_dividend = UINT64_C(23);
static volatile uint64_t runtime_divisor = UINT64_C(3);
static volatile uint64_t runtime_max = UINT64_MAX;
static volatile uint64_t runtime_signed_max = UINT64_C(9223372036854775807);
static volatile uint64_t runtime_high_equal = UINT64_C(9223372036854775808);

static checked_u64 checked_add_u64(uint64_t left, uint64_t right) {
    if (left > UINT64_MAX - right) {
        return (checked_u64){.ok = false, .value = 0};
    }
    return (checked_u64){.ok = true, .value = left + right};
}

static checked_u64 checked_subtract_u64(uint64_t left, uint64_t right) {
    if (left < right) {
        return (checked_u64){.ok = false, .value = 0};
    }
    return (checked_u64){.ok = true, .value = left - right};
}

static checked_u64 checked_multiply_u64(uint64_t left, uint64_t right) {
    if (left != 0 && right > UINT64_MAX / left) {
        return (checked_u64){.ok = false, .value = 0};
    }
    return (checked_u64){.ok = true, .value = left * right};
}

static checked_u64 checked_divide_u64(uint64_t left, uint64_t right) {
    if (right == 0) {
        return (checked_u64){.ok = false, .value = 0};
    }
    return (checked_u64){.ok = true, .value = left / right};
}

static checked_u64 checked_remainder_u64(uint64_t left, uint64_t right) {
    if (right == 0) {
        return (checked_u64){.ok = false, .value = 0};
    }
    return (checked_u64){.ok = true, .value = left % right};
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif

    const uint64_t high = runtime_high;
    const uint64_t increment = runtime_increment;
    const uint64_t one = runtime_one;
    const uint64_t product_left = runtime_product_left;
    const uint64_t product_right = runtime_product_right;
    const uint64_t dividend = runtime_dividend;
    const uint64_t divisor = runtime_divisor;
    const uint64_t max = runtime_max;
    const uint64_t signed_max = runtime_signed_max;
    const uint64_t high_equal = runtime_high_equal;

    const checked_u64 sum = checked_add_u64(high, increment);
    const checked_u64 difference = checked_subtract_u64(sum.value, one);
    const checked_u64 product = checked_multiply_u64(product_left, product_right);
    const checked_u64 quotient = checked_divide_u64(dividend, divisor);
    const checked_u64 remainder = checked_remainder_u64(dividend, divisor);
    const bool comparison_high = high > signed_max;
    const bool comparison_high_equal = high >= high_equal;
    const bool comparison_max = max > high;
    const bool comparison_high_less = high < max;
    const bool comparison_signed_equal = high == signed_max;
    const bool comparison_signed_different = high != signed_max;
    const bool comparison_less_equal = high <= high_equal;

    if (!sum.ok || !difference.ok || !product.ok || !quotient.ok ||
        !remainder.ok || !comparison_high || !comparison_high_equal ||
        !comparison_max || !comparison_high_less || comparison_signed_equal ||
        !comparison_signed_different || !comparison_less_equal) {
        return 1;
    }

    if (printf("UInt %" PRIu64 "/%" PRIu64 "/%" PRIu64
               "; div %" PRIu64 "; rem %" PRIu64
               "; cmp %s/%s/%s/%s/%s/%s/%s\n",
               sum.value, difference.value, product.value, quotient.value,
               remainder.value, comparison_high ? "true" : "false",
               comparison_high_equal ? "true" : "false",
               comparison_max ? "true" : "false",
               comparison_high_less ? "true" : "false",
               comparison_signed_equal ? "true" : "false",
               comparison_signed_different ? "true" : "false",
               comparison_less_equal ? "true" : "false") < 0) {
        return 1;
    }
    return 0;
}
