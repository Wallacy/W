// Rust 2024 reference for the public process-arguments-count executable workload.

use std::io::{self, Write};

fn main() {
    let count = std::env::args_os().count().saturating_sub(1);
    let mut stdout = io::stdout().lock();
    if writeln!(stdout, "Argument count {count}").is_err() {
        std::process::exit(1);
    }
}
