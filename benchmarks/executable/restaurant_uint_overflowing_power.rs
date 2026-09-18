// Rust 2024 reference for the Restaurant UInt overflowing-power workload.

use std::hint::black_box;
use std::io::{self, Write};

fn overflowing_power_u64(mut base: u64, mut exponent: u64) -> (u64, bool) {
    let mut value = 1_u64;
    let mut overflow = false;
    while exponent != 0 {
        if exponent & 1 != 0 {
            let (next, did_overflow) = value.overflowing_mul(base);
            value = next;
            overflow |= did_overflow;
        }
        exponent >>= 1;
        if exponent != 0 {
            let (next, did_overflow) = base.overflowing_mul(base);
            base = next;
            overflow |= did_overflow;
        }
    }
    (value, overflow)
}

fn main() {
    let ordinary = overflowing_power_u64(
        black_box(2_u64),
        black_box(63_u64),
    );
    let overflow = overflowing_power_u64(
        black_box(2_u64),
        black_box(64_u64),
    );
    let wrapped = overflowing_power_u64(
        black_box(u64::MAX),
        black_box(2_u64),
    );
    let identity = overflowing_power_u64(
        black_box(0_u64),
        black_box(0_u64),
    );
    let mut stdout = io::stdout().lock();
    if writeln!(
        stdout,
        "Overflowing power {}/{}; {}/{}; {}/{}; {}/{}",
        ordinary.0,
        ordinary.1,
        overflow.0,
        overflow.1,
        wrapped.0,
        wrapped.1,
        identity.0,
        identity.1,
    )
    .is_err()
    {
        std::process::exit(1);
    }
}
