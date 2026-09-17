// Rust 2024 reference for the Restaurant UInt wrapping-power workload.

use std::hint::black_box;
use std::io::{self, Write};

fn wrapping_power_u64(mut base: u64, mut exponent: u64) -> u64 {
    let mut result = 1_u64;
    while exponent != 0 {
        if exponent & 1 != 0 {
            result = result.wrapping_mul(base);
        }
        base = base.wrapping_mul(base);
        exponent >>= 1;
    }
    result
}

fn main() {
    let wrapped = wrapping_power_u64(black_box(3_u64), black_box(40_u64));
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Wrapped {wrapped}").is_err() {
        std::process::exit(1);
    }
}
