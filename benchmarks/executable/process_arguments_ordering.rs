// Rust 2024 reference for the public process-arguments-ordering executable workload.
// Expected output cases (argv => exit; stdout):
// [] => 0; "Argument mode compact: count=0\n"
// [""] => 0; "Argument mode compact: count=1\n"
// ["alpha", "beta"] => 0; "Argument mode extended: count=2\n"
// ["alpha", "beta", "gamma"] => 0; "Argument mode extended: count=3\n"

use std::io::{self, Write};

fn main() {
    let count = std::env::args_os().count().saturating_sub(1);
    let mut stdout = io::stdout().lock();
    let written = if count < 2 {
        writeln!(stdout, "Argument mode compact: count={count}")
    } else {
        writeln!(stdout, "Argument mode extended: count={count}")
    };
    if written.is_err() {
        std::process::exit(1);
    }
}
