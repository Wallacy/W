// Rust 2024 reference for the Restaurant checked UInt/u64 arithmetic workload.

use std::hint::black_box;
use std::io::{self, Write};

#[derive(Clone, Copy)]
struct CheckedU64 {
    ok: bool,
    value: u64,
}

fn checked_add_u64(left: u64, right: u64) -> CheckedU64 {
    match left.checked_add(right) {
        Some(value) => CheckedU64 { ok: true, value },
        None => CheckedU64 { ok: false, value: 0 },
    }
}

fn checked_subtract_u64(left: u64, right: u64) -> CheckedU64 {
    match left.checked_sub(right) {
        Some(value) => CheckedU64 { ok: true, value },
        None => CheckedU64 { ok: false, value: 0 },
    }
}

fn checked_multiply_u64(left: u64, right: u64) -> CheckedU64 {
    match left.checked_mul(right) {
        Some(value) => CheckedU64 { ok: true, value },
        None => CheckedU64 { ok: false, value: 0 },
    }
}

fn checked_divide_u64(left: u64, right: u64) -> CheckedU64 {
    match left.checked_div(right) {
        Some(value) => CheckedU64 { ok: true, value },
        None => CheckedU64 { ok: false, value: 0 },
    }
}

fn checked_remainder_u64(left: u64, right: u64) -> CheckedU64 {
    match left.checked_rem(right) {
        Some(value) => CheckedU64 { ok: true, value },
        None => CheckedU64 { ok: false, value: 0 },
    }
}

fn main() {
    // black_box keeps each reference operand runtime-dependent in Release.
    let high = black_box(9_223_372_036_854_775_808_u64);
    let increment = black_box(2_u64);
    let one = black_box(1_u64);
    let product_left = black_box(3_u64);
    let product_right = black_box(7_u64);
    let dividend = black_box(23_u64);
    let divisor = black_box(3_u64);
    let max = black_box(u64::MAX);
    let signed_max = black_box(i64::MAX as u64);
    let high_equal = black_box(9_223_372_036_854_775_808_u64);

    let sum = checked_add_u64(high, increment);
    let difference = checked_subtract_u64(sum.value, one);
    let product = checked_multiply_u64(product_left, product_right);
    let quotient = checked_divide_u64(dividend, divisor);
    let remainder = checked_remainder_u64(dividend, divisor);
    let comparison_high = high > signed_max;
    let comparison_high_equal = high >= high_equal;
    let comparison_max = max > high;
    let comparison_high_less = high < max;
    let comparison_signed_equal = high == signed_max;
    let comparison_signed_different = high != signed_max;
    let comparison_less_equal = high <= high_equal;

    if !sum.ok ||
        !difference.ok ||
        !product.ok ||
        !quotient.ok ||
        !remainder.ok ||
        !comparison_high ||
        !comparison_high_equal ||
        !comparison_max ||
        !comparison_high_less ||
        comparison_signed_equal ||
        !comparison_signed_different ||
        !comparison_less_equal
    {
        std::process::exit(1);
    }

    let mut stdout = io::stdout().lock();
    if writeln!(
        stdout,
        "UInt {}/{}/{}; div {}; rem {}; cmp {}/{}/{}/{}/{}/{}/{}",
        sum.value,
        difference.value,
        product.value,
        quotient.value,
        remainder.value,
        comparison_high,
        comparison_high_equal,
        comparison_max,
        comparison_high_less,
        comparison_signed_equal,
        comparison_signed_different,
        comparison_less_equal,
    )
    .is_err()
    {
        std::process::exit(1);
    }
}
