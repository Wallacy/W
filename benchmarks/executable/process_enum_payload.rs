// Rust 2024 reference for the public process-enum-payload executable workload.

use std::io::{self, Write};

enum AdmissionState {
    Unavailable,
    Observed { missing: bool, amount: i64 },
}

fn build_admission(missing: bool) -> AdmissionState {
    AdmissionState::Observed { missing, amount: 17 }
}

fn admission_is_missing(state: AdmissionState) -> bool {
    match state {
        AdmissionState::Unavailable => false,
        AdmissionState::Observed { missing, amount: _ } => missing,
    }
}

fn main() {
    let repeated = std::env::args_os().nth(1).is_none();
    let state = build_admission(repeated);
    let (label, exit_code) = if admission_is_missing(state) {
        ("enum-missing", 7)
    } else {
        ("enum-received", 0)
    };

    let mut stdout = io::stdout().lock();
    if write!(stdout, "{label} {repeated}\n").is_err() {
        std::process::exit(1);
    }
    std::process::exit(exit_code);
}
