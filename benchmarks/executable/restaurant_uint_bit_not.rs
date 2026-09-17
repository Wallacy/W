// Rust 2024 reference for the Restaurant UInt bitwise-complement workload.

use std::hint::black_box;
use std::io::{self, Write};

fn main() {
    let inverted = !black_box(0_u64);
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "UInt not {inverted}").is_err() {
        std::process::exit(1);
    }
}
