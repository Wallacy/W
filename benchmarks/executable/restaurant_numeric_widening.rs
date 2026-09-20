// Rust 2024 correctness reference for exact implicit numeric widening.
// Expected exit: 0
// Expected stdout:
// Numeric widen ok

use std::hint::black_box;

fn return_f32(value: i16) -> f32 {
    f32::from(value)
}

fn accept_f64(value: f64) -> f64 {
    value
}

fn main() {
    let signed_f32 = f32::from(black_box(-32767_i16));
    let unsigned_f32 = f32::from(black_box(u16::MAX));
    let returned_f32 = return_f32(black_box(-123_i16));
    let signed_f64 = f64::from(black_box(-2147483647_i32));
    let called_f64 = accept_f64(f64::from(black_box(u32::MAX)));
    let widened_float = f64::from(black_box(1.5_f32));
    let explicit_float = f64::from(black_box(1.25_f32));
    let mixed_integer = f64::from(black_box(2_i32)) + black_box(0.5_f64);
    let mixed_float = f64::from(black_box(1.5_f32)) + black_box(2.25_f64);
    let mixed_comparison = f32::from(black_box(u16::MAX)) == 65535.0_f32;
    let valid = signed_f32 == -32767.0_f32
        && unsigned_f32 == 65535.0_f32
        && returned_f32 == -123.0_f32
        && signed_f64 == -2147483647.0_f64
        && called_f64 == 4294967295.0_f64
        && widened_float == 1.5_f64
        && explicit_float == 1.25_f64
        && mixed_integer == 2.5_f64
        && mixed_float == 3.75_f64
        && mixed_comparison;
    println!("{}", if valid { "Numeric widen ok" } else { "Numeric widen bad" });
}
