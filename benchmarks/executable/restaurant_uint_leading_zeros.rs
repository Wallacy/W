// Rust 2024 reference for the Restaurant UInt leading-zeros workload.

use std::hint::black_box;
use std::io::{self, Write};

fn main() {
    let leading = black_box(0x00000000000000f0_u64).leading_zeros();
    let zero = black_box(0_u64).leading_zeros();
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Leading {leading}/{zero}").is_err() {
        std::process::exit(1);
    }
}
