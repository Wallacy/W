// Rust 2024 reference for the Restaurant UInt overflowing-add workload.

use std::hint::black_box;
use std::io::{self, Write};

fn overflowing_add_u64(left: u64, right: u64) -> (u64, bool) {
    left.overflowing_add(right)
}

fn main() {
    let (maximum, maximum_overflow) =
        overflowing_add_u64(black_box(u64::MAX), black_box(1_u64));
    let (ordinary, ordinary_overflow) =
        overflowing_add_u64(black_box(10_u64), black_box(1_u64));
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Overflowing {maximum}/{maximum_overflow}/{ordinary}/{ordinary_overflow}").is_err() {
        std::process::exit(1);
    }
}
