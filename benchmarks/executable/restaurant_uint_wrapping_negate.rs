// Rust 2024 reference for the Restaurant UInt wrapping-negate workload.

use std::hint::black_box;
use std::io::{self, Write};

fn wrapping_negate_u64(value: u64) -> u64 {
    value.wrapping_neg()
}

fn main() {
    let wrapped = wrapping_negate_u64(black_box(1_u64));
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Wrapped {wrapped}").is_err() {
        std::process::exit(1);
    }
}
