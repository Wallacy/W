// Rust 2024 reference for the Restaurant UInt masked-shift-right workload.

use std::hint::black_box;
use std::io::{self, Write};

fn masked_shift_right_u64(value: u64, count: u64) -> u64 {
    value.wrapping_shr(count as u32)
}

fn main() {
    let masked = masked_shift_right_u64(
        black_box(128_u64),
        black_box(65_u64),
    );
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Masked {masked}").is_err() {
        std::process::exit(1);
    }
}
