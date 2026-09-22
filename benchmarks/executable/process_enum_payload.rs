// Rust 2024 reference for the public process-enum-payload executable workload.
// Expected output cases (argv => exit; stdout):
// [] => 7; "arguments-missing count=0 amount=17 over-limit=false\n"
// [""] => 0; "arguments-present count=1 amount=17 over-limit=false\n"
// ["alpha", "beta"] => 0; "arguments-present count=2 amount=17 over-limit=false\n"
// ["alpha", "beta", "gamma"] => 0; "arguments-present count=3 amount=17 over-limit=true\n"

use std::io::{self, Write};

#[derive(Clone, Copy)]
enum ArgumentState {
    Unavailable,
    Observed { missing: bool, over_limit: bool, amount: i64 },
}

fn build_argument_state(missing: bool, over_limit: bool) -> ArgumentState {
    ArgumentState::Observed { missing, over_limit, amount: 17 }
}

fn argument_state_is_missing(state: ArgumentState) -> bool {
    match state {
        ArgumentState::Unavailable => false,
        ArgumentState::Observed { missing, over_limit: _, amount: _ } => missing,
    }
}

fn argument_state_is_over_limit(state: ArgumentState) -> bool {
    match state {
        ArgumentState::Unavailable => false,
        ArgumentState::Observed { missing: _, over_limit, amount: _ } => over_limit,
    }
}

fn argument_state_amount(state: ArgumentState) -> i64 {
    match state {
        ArgumentState::Unavailable => 0,
        ArgumentState::Observed { missing: _, over_limit: _, amount } => amount,
    }
}

fn main() {
    let count = std::env::args_os().count().saturating_sub(1);
    let missing = count == 0;
    let state = build_argument_state(missing, count > 2);
    let is_missing = argument_state_is_missing(state);
    let over_limit = argument_state_is_over_limit(state);
    let amount = argument_state_amount(state);
    let (label, exit_code) = if is_missing {
        ("arguments-missing", 7)
    } else {
        ("arguments-present", 0)
    };

    let mut stdout = io::stdout().lock();
    if write!(stdout, "{label} count={count} amount={amount} over-limit={over_limit}\n").is_err() {
        std::process::exit(1);
    }
    std::process::exit(exit_code);
}
