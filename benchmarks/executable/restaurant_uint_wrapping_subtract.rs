// Rust 2024 reference for the Restaurant UInt wrapping-subtract workload.

use std::hint::black_box;
use std::io::{self, Write};

fn wrapping_subtract_u64(left: u64, right: u64) -> u64 {
    left.wrapping_sub(right)
}

fn main() {
    let wrapped = wrapping_subtract_u64(black_box(0_u64), black_box(1_u64));
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Wrapped {wrapped}").is_err() {
        std::process::exit(1);
    }
}
