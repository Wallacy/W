// Rust 2024 reference for the Restaurant UInt reversed-bytes workload.

use std::hint::black_box;
use std::io::{self, Write};

fn main() {
    let reversed = black_box(0x0123_4567_89ab_cdef_u64).swap_bytes();
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Bytes {reversed}").is_err() {
        std::process::exit(1);
    }
}
