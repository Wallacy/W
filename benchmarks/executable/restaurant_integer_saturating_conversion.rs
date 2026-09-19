// Rust 2024 reference for representative Restaurant integer saturating conversions.
// Expected exit: 0
// Expected stdout:
// ss -128/7/127; us 7/127/127; su 0/200/255; uu 7/255/255; UInt->Int 9223372036854775807

use std::hint::black_box;

fn saturate_signed_to_signed_i8(value: i16) -> i8 {
    value.clamp(i8::MIN as i16, i8::MAX as i16) as i8
}

fn saturate_unsigned_to_signed_i8(value: u16) -> i8 {
    value.min(i8::MAX as u16) as i8
}

fn saturate_signed_to_unsigned_u8(value: i16) -> u8 {
    value.clamp(0, u8::MAX as i16) as u8
}

fn saturate_unsigned_to_unsigned_u8(value: u16) -> u8 {
    value.min(u8::MAX as u16) as u8
}

fn saturate_uint_to_int(value: u64) -> i64 {
    value.min(i64::MAX as u64) as i64
}

fn main() {
    println!(
        "ss {}/{}/{}; us {}/{}/{}; su {}/{}/{}; uu {}/{}/{}; UInt->Int {}",
        saturate_signed_to_signed_i8(black_box(-129_i16)),
        saturate_signed_to_signed_i8(black_box(7_i16)),
        saturate_signed_to_signed_i8(black_box(128_i16)),
        saturate_unsigned_to_signed_i8(black_box(7_u16)),
        saturate_unsigned_to_signed_i8(black_box(127_u16)),
        saturate_unsigned_to_signed_i8(black_box(128_u16)),
        saturate_signed_to_unsigned_u8(black_box(-1_i16)),
        saturate_signed_to_unsigned_u8(black_box(200_i16)),
        saturate_signed_to_unsigned_u8(black_box(256_i16)),
        saturate_unsigned_to_unsigned_u8(black_box(7_u16)),
        saturate_unsigned_to_unsigned_u8(black_box(255_u16)),
        saturate_unsigned_to_unsigned_u8(black_box(256_u16)),
        saturate_uint_to_int(black_box(u64::MAX)),
    );
}
