// Rust 2024 correctness reference for checked_integer_helper_fault; not a performance or CRT-free claim.
// Expected user-argument cases: 0 -> exit 0, stdout "Begin 255\n", empty stderr;
// 1 -> exit 2, empty stdout and stderr; 256 -> exit 1, empty stdout and stderr.

use std::process::ExitCode;

fn checked_add_u8(value: u8, rhs: u8) -> Option<u8> {
    value.checked_add(rhs)
}

fn main() -> ExitCode {
    let user_argument_count = std::env::args_os().skip(1).count();
    let Ok(value) = u8::try_from(user_argument_count) else {
        return ExitCode::from(1);
    };

    let Some(result) = checked_add_u8(value, u8::MAX) else {
        return ExitCode::from(2);
    };

    println!("Begin {result}");
    ExitCode::SUCCESS
}
