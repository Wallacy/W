// Rust 2024 reference for the Restaurant UInt rotated-left workload.

use std::hint::black_box;
use std::io::{self, Write};

fn rotated_left_u64(value: u64, count: u64) -> u64 {
    value.rotate_left((count & 63) as u32)
}

fn main() {
    let rotated = rotated_left_u64(
        black_box(0x8000000000000001_u64),
        black_box(1_u64),
    );
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Rotated {rotated}").is_err() {
        std::process::exit(1);
    }
}
