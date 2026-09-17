// Rust 2024 reference for the Restaurant UInt trailing-zeros workload.

use std::hint::black_box;
use std::io::{self, Write};

fn main() {
    let trailing = black_box(0x000000000000f000_u64).trailing_zeros();
    let zero = black_box(0_u64).trailing_zeros();
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Trailing {trailing}/{zero}").is_err() {
        std::process::exit(1);
    }
}
