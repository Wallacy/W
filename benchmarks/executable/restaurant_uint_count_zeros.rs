// Rust 2024 reference for the Restaurant UInt count-zeros workload.

use std::hint::black_box;
use std::io::{self, Write};

fn main() {
    let zeros = black_box(0xf0f0f0f00f0f0f0f_u64).count_zeros();
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Zeros {zeros}").is_err() {
        std::process::exit(1);
    }
}
