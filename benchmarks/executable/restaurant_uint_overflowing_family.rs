// Rust 2024 reference for the Restaurant UInt overflowing-family workload.
// Expected exit: 0
// Expected stdout:
// Overflowing family add 0/true,11/false; subtract 41/false,18446744073709551615/true; multiply 42/false,18446744073709551614/true; negate 0/false,18446744073709551615/true; power 9223372036854775808/false,0/true,1/true,1/false

use std::hint::black_box;
use std::io::{self, Write};

fn overflowing_add_u64(left: u64, right: u64) -> (u64, bool) {
    left.overflowing_add(right)
}

fn overflowing_subtract_u64(left: u64, right: u64) -> (u64, bool) {
    left.overflowing_sub(right)
}

fn overflowing_multiply_u64(left: u64, right: u64) -> (u64, bool) {
    left.overflowing_mul(right)
}

fn overflowing_negate_u64(value: u64) -> (u64, bool) {
    value.overflowing_neg()
}

fn overflowing_power_u64(mut base: u64, mut exponent: u64) -> (u64, bool) {
    let mut value = 1_u64;
    let mut overflow = false;
    while exponent != 0 {
        if exponent & 1 != 0 {
            let (next, did_overflow) = value.overflowing_mul(base);
            value = next;
            overflow |= did_overflow;
        }
        exponent >>= 1;
        if exponent != 0 {
            let (next, did_overflow) = base.overflowing_mul(base);
            base = next;
            overflow |= did_overflow;
        }
    }
    (value, overflow)
}

fn main() {
    let maximum_add = overflowing_add_u64(black_box(u64::MAX), black_box(1_u64));
    let ordinary_add = overflowing_add_u64(black_box(10_u64), black_box(1_u64));
    let ordinary_subtract = overflowing_subtract_u64(
        black_box(42_u64),
        black_box(1_u64),
    );
    let underflow = overflowing_subtract_u64(
        black_box(0_u64),
        black_box(1_u64),
    );
    let ordinary_multiply = overflowing_multiply_u64(
        black_box(6_u64),
        black_box(7_u64),
    );
    let overflow = overflowing_multiply_u64(
        black_box(u64::MAX),
        black_box(2_u64),
    );
    let zero_negate = overflowing_negate_u64(black_box(0_u64));
    let one_negate = overflowing_negate_u64(black_box(1_u64));
    let ordinary_power = overflowing_power_u64(black_box(2_u64), black_box(63_u64));
    let overflow_power = overflowing_power_u64(black_box(2_u64), black_box(64_u64));
    let wrapped_power = overflowing_power_u64(black_box(u64::MAX), black_box(2_u64));
    let identity_power = overflowing_power_u64(black_box(0_u64), black_box(0_u64));
    let mut stdout = io::stdout().lock();
    if writeln!(
        stdout,
        "Overflowing family add {}/{},{}/{}; subtract {}/{},{}/{}; multiply {}/{},{}/{}; negate {}/{},{}/{}; power {}/{},{}/{},{}/{},{}/{}",
        maximum_add.0,
        maximum_add.1,
        ordinary_add.0,
        ordinary_add.1,
        ordinary_subtract.0,
        ordinary_subtract.1,
        underflow.0,
        underflow.1,
        ordinary_multiply.0,
        ordinary_multiply.1,
        overflow.0,
        overflow.1,
        zero_negate.0,
        zero_negate.1,
        one_negate.0,
        one_negate.1,
        ordinary_power.0,
        ordinary_power.1,
        overflow_power.0,
        overflow_power.1,
        wrapped_power.0,
        wrapped_power.1,
        identity_power.0,
        identity_power.1,
    )
    .is_err()
    {
        std::process::exit(1);
    }
}
