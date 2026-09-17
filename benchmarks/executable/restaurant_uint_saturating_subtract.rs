// Rust 2024 reference for the Restaurant UInt saturating-subtract workload.

use std::hint::black_box;
use std::io::{self, Write};

fn saturating_subtract_u64(value: u64, amount: u64) -> u64 {
    value.saturating_sub(amount)
}

fn main() {
    let zero = saturating_subtract_u64(black_box(0_u64), black_box(1_u64));
    let ordinary = saturating_subtract_u64(black_box(11_u64), black_box(1_u64));
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Saturated subtract {zero}/{ordinary}").is_err() {
        std::process::exit(1);
    }
}
