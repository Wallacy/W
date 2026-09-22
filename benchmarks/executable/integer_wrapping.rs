// Rust 2024 reference for the Restaurant fixed-width integer wrapping workload.
// Expected exit: 0
// Expected stdout:
// i8/u8 -128/0
// i16/u16 32767/2
// i32/u32 -2/4294967295
// i64/u64 -9223372036854775808/0
// Int/UInt -9223372036854775808/18446744073709551615

use std::hint::black_box;
use std::io::{self, Write};

fn wrapping_power_i64(base: i64, mut exponent: u64) -> i64 {
    let mut current = u64::from_ne_bytes(base.to_ne_bytes());
    let mut result = 1_u64;
    while exponent != 0 {
        if exponent & 1 != 0 {
            result = result.wrapping_mul(current);
        }
        exponent >>= 1;
        if exponent != 0 {
            current = current.wrapping_mul(current);
        }
    }
    i64::from_ne_bytes(result.to_ne_bytes())
}

fn main() {
    let signed8 = black_box(i8::MAX).wrapping_add(black_box(1_i8));
    let signed16 = black_box(-32767_i16).wrapping_sub(black_box(2_i16));
    let signed32 = black_box(i32::MAX).wrapping_mul(black_box(2_i32));
    let signed64_min = black_box(i64::MAX).wrapping_add(black_box(1_i64));
    let signed64 = black_box(signed64_min).wrapping_neg();
    let unsigned8 = black_box(2_u8).wrapping_pow(black_box(8_u32));
    let unsigned16 = black_box(0x8001_u16).wrapping_shl(black_box(1_u32));
    let unsigned32 = black_box(1_u32).wrapping_neg();
    let unsigned64 = black_box(u64::MAX).wrapping_add(black_box(1_u64));
    let signed_alias = wrapping_power_i64(black_box(-2_i64), black_box(63_u64));
    let unsigned_alias = black_box(0_u64).wrapping_sub(black_box(1_u64));

    let mut stdout = io::stdout().lock();
    if writeln!(
        stdout,
        concat!(
            "i8/u8 {}/{}\n",
            "i16/u16 {}/{}\n",
            "i32/u32 {}/{}\n",
            "i64/u64 {}/{}\n",
            "Int/UInt {}/{}",
        ),
        signed8,
        unsigned8,
        signed16,
        unsigned16,
        signed32,
        unsigned32,
        signed64,
        unsigned64,
        signed_alias,
        unsigned_alias,
    )
    .is_err()
    {
        std::process::exit(1);
    }
}
