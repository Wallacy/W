// Rust 2024 reference for the Restaurant UInt wrapping-add workload.

use std::hint::black_box;
use std::io::{self, Write};

fn wrapping_add_u64(left: u64, right: u64) -> u64 {
    left.wrapping_add(right)
}

fn main() {
    let wrapped = wrapping_add_u64(black_box(u64::MAX), black_box(1_u64));
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Wrapped {wrapped}").is_err() {
        std::process::exit(1);
    }
}
