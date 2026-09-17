// Rust 2024 reference for the Restaurant UInt logical-shift-right workload.

use std::hint::black_box;
use std::io::{self, Write};

fn logical_shift_right_u64(value: u64, count: u64) -> Option<u64> {
    let count = u32::try_from(count).ok()?;
    if count >= u64::BITS {
        return None;
    }
    Some(value >> count)
}

fn main() {
    let Some(logical) = logical_shift_right_u64(
        black_box(128_u64),
        black_box(1_u64),
    ) else {
        std::process::exit(1);
    };
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Logical {logical}").is_err() {
        std::process::exit(1);
    }
}
