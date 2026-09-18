// Rust 2024 reference for the Restaurant UInt saturating-policy workload.

use std::hint::black_box;
use std::io::{self, Write};

fn saturating_negate_u64(value: u64) -> u64 {
    0_u64.saturating_sub(value)
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
    let neg_zero = saturating_negate_u64(black_box(0_u64));
    let neg_maximum = saturating_negate_u64(black_box(u64::MAX));
    let ordinary = saturating_power_u64(black_box(2_u64), black_box(3_u64));
    let clamped = saturating_power_u64(black_box(2_u64), black_box(64_u64));
    let identity = saturating_power_u64(black_box(0_u64), black_box(0_u64));
    let mut stdout = io::stdout().lock();
    if writeln!(
        stdout,
        "Saturating policy {neg_zero}/{neg_maximum}/{ordinary}/{clamped}/{identity}",
    )
    .is_err()
    {
        std::process::exit(1);
    }
}
