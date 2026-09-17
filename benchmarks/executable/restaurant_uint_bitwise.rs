// Rust 2024 reference for the Restaurant UInt binary-bitwise workload.

use std::hint::black_box;
use std::io::{self, Write};

fn main() {
    let left = black_box(9_223_372_036_854_775_808_u64);
    let right = black_box(9_223_372_036_854_775_807_u64);
    let combined = left | (right ^ (left & right));
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "UInt bits {combined}").is_err() {
        std::process::exit(1);
    }
}
