// Rust 2024 correctness reference for f32/f64 bit-preserving round trips.
// Expected exit: 0
// Expected stdout:
// Float bits f32 2147483648/2139095040/2143363909 f64 9223372036854775808/9218868437227405312/9221140253039434428

use std::hint::black_box;

fn main() {
    let f32_negative_zero = f32::from_bits(black_box(0x80000000_u32)).to_bits();
    let f32_infinity = f32::from_bits(black_box(0x7f800000_u32)).to_bits();
    let f32_nan = f32::from_bits(black_box(0x7fc12345_u32)).to_bits();
    let f64_negative_zero = f64::from_bits(black_box(0x8000000000000000_u64)).to_bits();
    let f64_infinity = f64::from_bits(black_box(0x7ff0000000000000_u64)).to_bits();
    let f64_nan = f64::from_bits(black_box(0x7ff8123456789abc_u64)).to_bits();

    println!(
        "Float bits f32 {}/{}/{} f64 {}/{}/{}",
        f32_negative_zero,
        f32_infinity,
        f32_nan,
        f64_negative_zero,
        f64_infinity,
        f64_nan,
    );
}
