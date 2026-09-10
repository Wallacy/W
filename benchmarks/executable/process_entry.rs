// Rust 2024 reference for the public process-entry executable workload.

use std::env;
use std::io::{self, Write};

fn main() {
    let has_user_argument = env::args_os().nth(1).is_some();
    let (output, exit_code) = if has_user_argument {
        (b"received\n".as_slice(), 0)
    } else {
        (b"missing\n".as_slice(), 2)
    };

    let mut stdout = io::stdout().lock();
    if stdout.write_all(output).is_err() {
        std::process::exit(1);
    }
    std::process::exit(exit_code);
}
