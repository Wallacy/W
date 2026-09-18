// Rust 2024 reference for the Restaurant UInt overflowing-family workload.

use std::hint::black_box;
use std::io::{self, Write};

fn overflowing_subtract_u64(left: u64, right: u64) -> (u64, bool) {
    left.overflowing_sub(right)
}

fn overflowing_multiply_u64(left: u64, right: u64) -> (u64, bool) {
    left.overflowing_mul(right)
}

fn overflowing_negate_u64(value: u64) -> (u64, bool) {
    value.overflowing_neg()
}

fn main() {
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
    let mut stdout = io::stdout().lock();
    if writeln!(
        stdout,
        "Overflowing family {}/{}/{}/{}/{}/{}/{}/{}/{}/{}/{}/{}",
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
    )
    .is_err()
    {
        std::process::exit(1);
    }
}
