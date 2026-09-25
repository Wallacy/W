// Rust 2024 correctness reference for checked_scalar_if_join; not a performance or CRT-free claim.
// Expected output cases (argv => exit; stdout):
// [] => 0; "Joined -1\n"
// ["x"] => 0; "Joined 0\n"
// ["x", "x"] => 2; ""
// ["x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x","x"] => 1; ""

use std::process::ExitCode;

fn choose(value: i8) -> Option<i8> {
    if value == 2 {
        value.checked_mul(127)
    } else {
        value.checked_sub(1)
    }
}

fn main() -> ExitCode {
    let user_argument_count = std::env::args_os().skip(1).count();
    let Ok(value) = i8::try_from(user_argument_count) else {
        return ExitCode::from(1);
    };

    let Some(result) = choose(value) else {
        return ExitCode::from(2);
    };

    println!("Joined {result}");
    ExitCode::SUCCESS
}
