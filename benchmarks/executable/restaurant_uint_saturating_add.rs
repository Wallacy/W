// Rust 2024 reference for the Restaurant UInt saturating-add workload.

use std::hint::black_box;
use std::io::{self, Write};

fn saturating_add_u64(left: u64, right: u64) -> u64 {
    left.saturating_add(right)
}

fn main() {
    let maximum = saturating_add_u64(black_box(u64::MAX), black_box(1_u64));
    let ordinary = saturating_add_u64(black_box(10_u64), black_box(1_u64));
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Saturated {maximum}/{ordinary}").is_err() {
        std::process::exit(1);
    }
}
