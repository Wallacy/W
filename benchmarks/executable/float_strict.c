// C23 correctness reference for the Restaurant strict binary32/binary64 family.
// Expected exit: 0
// Expected stdout:
// Float strict ok

#include <stdio.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static volatile float f32_sum_left = 1.5f;
static volatile float f32_sum_right = 2.25f;
static volatile float f32_difference_left = 9.5f;
static volatile float f32_difference_right = 5.5f;
static volatile float f32_product_left = 1.5f;
static volatile float f32_product_right = 2.0f;
static volatile float f32_quotient_left = 7.5f;
static volatile float f32_quotient_right = 2.5f;
static volatile float f32_zero = 0.0f;

static volatile double f64_sum_left = 1.5;
static volatile double f64_sum_right = 2.25;
static volatile double f64_difference_left = 9.5;
static volatile double f64_difference_right = 5.5;
static volatile double f64_product_left = 1.5;
static volatile double f64_product_right = 2.0;
static volatile double f64_quotient_left = 7.5;
static volatile double f64_quotient_right = 2.5;
static volatile double f64_zero = 0.0;

static int strict_f32_check(void) {
    const float sum = f32_sum_left + f32_sum_right;
    const float difference = f32_difference_left - f32_difference_right;
    const float product = f32_product_left * f32_product_right;
    const float quotient = f32_quotient_left / f32_quotient_right;
    const float signed_zero = -f32_zero;
    const float nan = f32_zero / f32_zero;
    return sum == 3.75f && difference == 4.0f && product == 3.0f &&
           quotient != 4.0f && sum > 3.0f && sum >= 3.75f &&
           quotient < 4.0f && quotient <= 3.0f && signed_zero == 0.0f &&
           nan != nan;
}

static int strict_f64_check(void) {
    const double sum = f64_sum_left + f64_sum_right;
    const double difference = f64_difference_left - f64_difference_right;
    const double product = f64_product_left * f64_product_right;
    const double quotient = f64_quotient_left / f64_quotient_right;
    const double signed_zero = -f64_zero;
    const double nan = f64_zero / f64_zero;
    return sum == 3.75 && difference == 4.0 && product == 3.0 &&
           quotient != 4.0 && sum > 3.0 && sum >= 3.75 &&
           quotient < 4.0 && quotient <= 3.0 && signed_zero == 0.0 &&
           nan != nan;
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 1;
#endif
    if (fputs(strict_f32_check() && strict_f64_check()
                  ? "Float strict ok\n"
                  : "Float strict bad\n",
              stdout) == EOF)
        return 1;
    return 0;
}
