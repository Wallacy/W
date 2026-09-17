// Rust 2024 reference for the Restaurant UInt reversed-bits workload.

use std::hint::black_box;
use std::io::{self, Write};

fn main() {
    let reversed = black_box(0x0123_4567_89ab_cdef_u64).reverse_bits();
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Bits {reversed}").is_err() {
        std::process::exit(1);
    }
}
