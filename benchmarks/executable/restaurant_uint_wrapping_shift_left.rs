// Rust 2024 reference for the Restaurant UInt wrapping-shift-left workload.

use std::hint::black_box;
use std::io::{self, Write};

fn wrapping_shift_left_u64(value: u64, count: u64) -> Option<u64> {
    let count = u32::try_from(count).ok()?;
    if count >= u64::BITS {
        return None;
    }
    Some(value.wrapping_shl(count))
}

fn main() {
    let Some(wrapped) = wrapping_shift_left_u64(
        black_box(u64::MAX),
        black_box(1_u64),
    ) else {
        std::process::exit(1);
    };
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Wrapped {wrapped}").is_err() {
        std::process::exit(1);
    }
}
