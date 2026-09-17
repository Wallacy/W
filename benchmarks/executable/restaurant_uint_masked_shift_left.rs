// Rust 2024 reference for the Restaurant UInt masked-shift-left workload.

use std::hint::black_box;
use std::io::{self, Write};

fn masked_shift_left_u64(value: u64, count: u64) -> u64 {
    value.wrapping_shl(count as u32)
}

fn main() {
    let masked = masked_shift_left_u64(
        black_box(1_u64),
        black_box(65_u64),
    );
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Masked {masked}").is_err() {
        std::process::exit(1);
    }
}
