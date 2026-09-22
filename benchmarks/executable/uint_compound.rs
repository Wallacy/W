use std::hint::black_box;
use std::io::{self, Write};

fn checked_power_u64(mut base: u64, mut exponent: u64) -> Option<u64> {
    let mut result = 1_u64;
    while exponent != 0 {
        if exponent & 1 != 0 {
            result = result.checked_mul(base)?;
        }
        exponent >>= 1;
        if exponent != 0 {
            base = base.checked_mul(base)?;
        }
    }
    Some(result)
}

fn checked_shift_left_u64(value: u64, count: u64) -> Option<u64> {
    let count = u32::try_from(count).ok()?;
    if count >= 64 || (count != 0 && value >> (64 - count) != 0) {
        return None;
    }
    Some(value << count)
}

fn checked_shift_right_u64(value: u64, count: u64) -> Option<u64> {
    value.checked_shr(u32::try_from(count).ok()?)
}

fn main() {
    let mut value = black_box(4_611_686_018_427_387_904_u64);
    value = value.checked_add(black_box(3_u64)).unwrap_or_else(|| std::process::exit(1));
    let after_add = value;
    value = value.checked_sub(black_box(1_u64)).unwrap_or_else(|| std::process::exit(1));
    let after_subtract = value;
    value = value.checked_mul(black_box(2_u64)).unwrap_or_else(|| std::process::exit(1));
    let after_multiply = value;
    value = value.checked_div(black_box(2_u64)).unwrap_or_else(|| std::process::exit(1));
    let after_divide = value;
    value = value.checked_rem(black_box(u64::MAX)).unwrap_or_else(|| std::process::exit(1));
    let after_remainder = value;
    value = checked_power_u64(value, black_box(1_u64)).unwrap_or_else(|| std::process::exit(1));
    let after_power = value;
    value = checked_shift_left_u64(value, black_box(1_u64)).unwrap_or_else(|| std::process::exit(1));
    let after_shift_left = value;
    value = checked_shift_right_u64(value, black_box(1_u64)).unwrap_or_else(|| std::process::exit(1));
    let after_shift_right = value;
    value &= black_box(255_u64);
    let after_and = value;
    value ^= black_box(85_u64);
    let after_xor = value;
    value |= black_box(10_u64);
    let mut stdout = io::stdout().lock();
    if writeln!(
        stdout,
        "UInt compound {after_add}/{after_subtract}/{after_multiply}/{after_divide}/{after_remainder}/{after_power}/{after_shift_left}/{after_shift_right}/{after_and}/{after_xor}/{value}"
    )
    .is_err()
    {
        std::process::exit(1);
    }
}
