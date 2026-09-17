use std::hint::black_box;
use std::io::{self, Write};

fn main() {
    let mut value = black_box(u64::MAX);
    value &= black_box(240_u64);
    value ^= black_box(170_u64);
    value |= black_box(5_u64);
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "UInt compound {value}").is_err() {
        std::process::exit(1);
    }
}
