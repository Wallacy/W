// Rust 2024 reference for the Restaurant UInt saturating-multiply workload.

use std::hint::black_box;
use std::io::{self, Write};

fn saturating_multiply_u64(left: u64, right: u64) -> u64 {
    left.saturating_mul(right)
}

fn main() {
    let maximum = saturating_multiply_u64(black_box(u64::MAX), black_box(2_u64));
    let ordinary = saturating_multiply_u64(black_box(6_u64), black_box(7_u64));
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Saturated multiply {maximum}/{ordinary}").is_err() {
        std::process::exit(1);
    }
}
