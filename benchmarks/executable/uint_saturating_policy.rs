// Rust 2024 reference for the Restaurant UInt saturating-policy workload.
// Expected exit: 0
// Expected stdout:
// Saturating policy add 18446744073709551615/11; subtract 0/10; multiply 18446744073709551615/42; negate 0/0; power 8/18446744073709551615/1

use std::hint::black_box;
use std::io::{self, Write};

fn saturating_add_u64(left: u64, right: u64) -> u64 {
    left.saturating_add(right)
}

fn saturating_subtract_u64(value: u64, amount: u64) -> u64 {
    value.saturating_sub(amount)
}

fn saturating_negate_u64(value: u64) -> u64 {
    0_u64.saturating_sub(value)
}

fn saturating_multiply_u64(left: u64, right: u64) -> u64 {
    left.saturating_mul(right)
}

fn saturating_power_u64(mut base: u64, mut exponent: u64) -> u64 {
    let mut result = 1_u64;
    while exponent != 0 {
        if exponent & 1 != 0 {
            result = result.saturating_mul(base);
        }
        exponent >>= 1;
        if exponent != 0 {
            base = base.saturating_mul(base);
        }
    }
    result
}

fn main() {
    let maximum_add = saturating_add_u64(black_box(u64::MAX), black_box(1_u64));
    let ordinary_add = saturating_add_u64(black_box(10_u64), black_box(1_u64));
    let underflow_subtract =
        saturating_subtract_u64(black_box(0_u64), black_box(1_u64));
    let ordinary_subtract =
        saturating_subtract_u64(black_box(11_u64), black_box(1_u64));
    let overflow_multiply =
        saturating_multiply_u64(black_box(u64::MAX), black_box(2_u64));
    let ordinary_multiply =
        saturating_multiply_u64(black_box(6_u64), black_box(7_u64));
    let neg_zero = saturating_negate_u64(black_box(0_u64));
    let neg_maximum = saturating_negate_u64(black_box(u64::MAX));
    let ordinary = saturating_power_u64(black_box(2_u64), black_box(3_u64));
    let clamped = saturating_power_u64(black_box(2_u64), black_box(64_u64));
    let identity = saturating_power_u64(black_box(0_u64), black_box(0_u64));
    let mut stdout = io::stdout().lock();
    if writeln!(
        stdout,
        "Saturating policy add {maximum_add}/{ordinary_add}; subtract {underflow_subtract}/{ordinary_subtract}; multiply {overflow_multiply}/{ordinary_multiply}; negate {neg_zero}/{neg_maximum}; power {ordinary}/{clamped}/{identity}",
    )
    .is_err()
    {
        std::process::exit(1);
    }
}
