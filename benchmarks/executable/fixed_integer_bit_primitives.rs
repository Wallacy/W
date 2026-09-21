// Rust 2024 correctness reference for the fixed-width W bit-primitives family.
// Expected exit: 0
// Expected stdout:
// i8 3/5/1/1 74/127 82 -92/41
// u8 4/4/0/1 105 150 45/75
// i16 5/11/3/2 11336/32767 13330 9320/2330
// u16 8/8/0/0 54673 43913 4951/50389
// i32 13/19/3/3 510274632/2147483647 2018915346 610839792/152709948
// u32 20/12/0/0 4155757969 4023233417 324508639/3302352631
// i64 30/34/7/1 8553414939923104896/9223372036854775807 7984226321029210881 163971058432973532/40992764608243383
// u64 32/32/0/4 597899502893742975 1167088121787636990 18282773015276577825/9182379272246532360

use std::hint::black_box;
use std::io::{self, Write};

macro_rules! signed_row_values {
    ($type:ty, $value:expr, $width:expr) => {{
        let value: $type = black_box($value);
        let zero: $type = black_box(0);
        let negative: $type = black_box(-2);
        let rotations = [
            black_box(0_u32),
            black_box($width as u32),
            black_box($width as u32 + 1),
        ];
        (
            value.count_ones(),
            value.count_zeros(),
            value.leading_zeros(),
            value.trailing_zeros(),
            zero.leading_zeros(),
            zero.trailing_zeros(),
            value.reverse_bits(),
            negative.reverse_bits(),
            value.swap_bytes(),
            value.rotate_left(rotations[0]),
            value.rotate_left(rotations[1]),
            value.rotate_left(rotations[2]),
            value.rotate_right(rotations[0]),
            value.rotate_right(rotations[1]),
            value.rotate_right(rotations[2]),
        )
    }};
}

macro_rules! unsigned_row_values {
    ($type:ty, $value:expr, $width:expr) => {{
        let value: $type = black_box($value);
        let zero: $type = black_box(0);
        let rotations = [
            black_box(0_u32),
            black_box($width as u32),
            black_box($width as u32 + 1),
        ];
        (
            value.count_ones(),
            value.count_zeros(),
            value.leading_zeros(),
            value.trailing_zeros(),
            zero.leading_zeros(),
            zero.trailing_zeros(),
            value.reverse_bits(),
            value.swap_bytes(),
            value.rotate_left(rotations[0]),
            value.rotate_left(rotations[1]),
            value.rotate_left(rotations[2]),
            value.rotate_right(rotations[0]),
            value.rotate_right(rotations[1]),
            value.rotate_right(rotations[2]),
        )
    }};
}

macro_rules! assert_signed_edges {
    ($leading:expr, $trailing:expr, $left0:expr, $left_width:expr,
     $right0:expr, $right_width:expr, $width:expr, $value:expr) => {
        assert_eq!(($leading, $trailing), ($width as u32, $width as u32));
        assert_eq!(
            ($left0, $left_width, $right0, $right_width),
            ($value, $value, $value, $value),
        );
    };
}

macro_rules! assert_unsigned_edges {
    ($leading:expr, $trailing:expr, $left0:expr, $left_width:expr,
     $right0:expr, $right_width:expr, $width:expr, $value:expr) => {
        assert_eq!(($leading, $trailing), ($width as u32, $width as u32));
        assert_eq!(
            ($left0, $left_width, $right0, $right_width),
            ($value, $value, $value, $value),
        );
    };
}

fn main() {
    let (i8_ones, i8_zeros, i8_leading, i8_trailing, i8_leading_zero,
        i8_trailing_zero, i8_bits, i8_negative_bits, i8_bytes, i8_left0,
        i8_left_width, i8_left_width_plus_one, i8_right0, i8_right_width,
        i8_right_width_plus_one) = signed_row_values!(i8, 0x52_i8, 8);
    let (u8_ones, u8_zeros, u8_leading, u8_trailing, u8_leading_zero,
        u8_trailing_zero, u8_bits, u8_bytes, u8_left0, u8_left_width,
        u8_left_width_plus_one, u8_right0, u8_right_width,
        u8_right_width_plus_one) = unsigned_row_values!(u8, 0x96_u8, 8);
    let (i16_ones, i16_zeros, i16_leading, i16_trailing, i16_leading_zero,
        i16_trailing_zero, i16_bits, i16_negative_bits, i16_bytes, i16_left0,
        i16_left_width, i16_left_width_plus_one, i16_right0, i16_right_width,
        i16_right_width_plus_one) = signed_row_values!(i16, 0x1234_i16, 16);
    let (u16_ones, u16_zeros, u16_leading, u16_trailing, u16_leading_zero,
        u16_trailing_zero, u16_bits, u16_bytes, u16_left0, u16_left_width,
        u16_left_width_plus_one, u16_right0, u16_right_width,
        u16_right_width_plus_one) = unsigned_row_values!(u16, 0x89ab_u16, 16);
    let (i32_ones, i32_zeros, i32_leading, i32_trailing, i32_leading_zero,
        i32_trailing_zero, i32_bits, i32_negative_bits, i32_bytes, i32_left0,
        i32_left_width, i32_left_width_plus_one, i32_right0, i32_right_width,
        i32_right_width_plus_one) = signed_row_values!(i32, 0x12345678_i32, 32);
    let (u32_ones, u32_zeros, u32_leading, u32_trailing, u32_leading_zero,
        u32_trailing_zero, u32_bits, u32_bytes, u32_left0, u32_left_width,
        u32_left_width_plus_one, u32_right0, u32_right_width,
        u32_right_width_plus_one) = unsigned_row_values!(u32, 0x89abcdef_u32, 32);
    let (i64_ones, i64_zeros, i64_leading, i64_trailing, i64_leading_zero,
        i64_trailing_zero, i64_bits, i64_negative_bits, i64_bytes, i64_left0,
        i64_left_width, i64_left_width_plus_one, i64_right0, i64_right_width,
        i64_right_width_plus_one) =
        signed_row_values!(i64, 0x0123456789abcd6e_i64, 64);
    let (u64_ones, u64_zeros, u64_leading, u64_trailing, u64_leading_zero,
        u64_trailing_zero, u64_bits, u64_bytes, u64_left0, u64_left_width,
        u64_left_width_plus_one, u64_right0, u64_right_width,
        u64_right_width_plus_one) =
        unsigned_row_values!(u64, 0xfedcba9876543210_u64, 64);

    assert_signed_edges!(i8_leading_zero, i8_trailing_zero, i8_left0,
        i8_left_width, i8_right0, i8_right_width, 8, 0x52_i8);
    assert_unsigned_edges!(u8_leading_zero, u8_trailing_zero, u8_left0,
        u8_left_width, u8_right0, u8_right_width, 8, 0x96_u8);
    assert_signed_edges!(i16_leading_zero, i16_trailing_zero, i16_left0,
        i16_left_width, i16_right0, i16_right_width, 16, 0x1234_i16);
    assert_unsigned_edges!(u16_leading_zero, u16_trailing_zero, u16_left0,
        u16_left_width, u16_right0, u16_right_width, 16, 0x89ab_u16);
    assert_signed_edges!(i32_leading_zero, i32_trailing_zero, i32_left0,
        i32_left_width, i32_right0, i32_right_width, 32, 0x12345678_i32);
    assert_unsigned_edges!(u32_leading_zero, u32_trailing_zero, u32_left0,
        u32_left_width, u32_right0, u32_right_width, 32, 0x89abcdef_u32);
    assert_signed_edges!(i64_leading_zero, i64_trailing_zero, i64_left0,
        i64_left_width, i64_right0, i64_right_width, 64,
        0x0123456789abcd6e_i64);
    assert_unsigned_edges!(u64_leading_zero, u64_trailing_zero, u64_left0,
        u64_left_width, u64_right0, u64_right_width, 64,
        0xfedcba9876543210_u64);

    let mut stdout = io::stdout().lock();
    if write!(
        stdout,
        concat!(
            "i8 {}/{}/{}/{} {}/{} {} {}/{}\n",
            "u8 {}/{}/{}/{} {} {} {}/{}\n",
            "i16 {}/{}/{}/{} {}/{} {} {}/{}\n",
            "u16 {}/{}/{}/{} {} {} {}/{}\n",
            "i32 {}/{}/{}/{} {}/{} {} {}/{}\n",
            "u32 {}/{}/{}/{} {} {} {}/{}\n",
            "i64 {}/{}/{}/{} {}/{} {} {}/{}\n",
            "u64 {}/{}/{}/{} {} {} {}/{}\n",
        ),
        i8_ones, i8_zeros, i8_leading, i8_trailing, i8_bits, i8_negative_bits,
        i8_bytes, i8_left_width_plus_one, i8_right_width_plus_one,
        u8_ones, u8_zeros, u8_leading, u8_trailing, u8_bits, u8_bytes,
        u8_left_width_plus_one, u8_right_width_plus_one,
        i16_ones, i16_zeros, i16_leading, i16_trailing, i16_bits,
        i16_negative_bits, i16_bytes, i16_left_width_plus_one,
        i16_right_width_plus_one,
        u16_ones, u16_zeros, u16_leading, u16_trailing, u16_bits, u16_bytes,
        u16_left_width_plus_one, u16_right_width_plus_one,
        i32_ones, i32_zeros, i32_leading, i32_trailing, i32_bits,
        i32_negative_bits, i32_bytes, i32_left_width_plus_one,
        i32_right_width_plus_one,
        u32_ones, u32_zeros, u32_leading, u32_trailing, u32_bits, u32_bytes,
        u32_left_width_plus_one, u32_right_width_plus_one,
        i64_ones, i64_zeros, i64_leading, i64_trailing, i64_bits,
        i64_negative_bits, i64_bytes, i64_left_width_plus_one,
        i64_right_width_plus_one,
        u64_ones, u64_zeros, u64_leading, u64_trailing, u64_bits, u64_bytes,
        u64_left_width_plus_one, u64_right_width_plus_one,
    )
    .is_err()
    {
        std::process::exit(1);
    }
}
