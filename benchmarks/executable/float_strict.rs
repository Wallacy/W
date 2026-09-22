// Rust 2024 correctness reference for the Restaurant strict binary32/binary64 family.
// Expected exit: 0
// Expected stdout:
// Float strict ok

use std::hint::black_box;

fn strict_f32_check() -> bool {
    let sum = black_box(1.5_f32) + black_box(2.25_f32);
    let difference = black_box(9.5_f32) - black_box(5.5_f32);
    let product = black_box(1.5_f32) * black_box(2.0_f32);
    let quotient = black_box(7.5_f32) / black_box(2.5_f32);
    let signed_zero = -black_box(0.0_f32);
    let nan = black_box(0.0_f32) / black_box(0.0_f32);
    sum == 3.75_f32
        && difference == 4.0_f32
        && product == 3.0_f32
        && quotient != 4.0_f32
        && sum > 3.0_f32
        && sum >= 3.75_f32
        && quotient < 4.0_f32
        && quotient <= 3.0_f32
        && signed_zero == 0.0_f32
        && nan != nan
}

fn strict_f64_check() -> bool {
    let sum = black_box(1.5_f64) + black_box(2.25_f64);
    let difference = black_box(9.5_f64) - black_box(5.5_f64);
    let product = black_box(1.5_f64) * black_box(2.0_f64);
    let quotient = black_box(7.5_f64) / black_box(2.5_f64);
    let signed_zero = -black_box(0.0_f64);
    let nan = black_box(0.0_f64) / black_box(0.0_f64);
    sum == 3.75_f64
        && difference == 4.0_f64
        && product == 3.0_f64
        && quotient != 4.0_f64
        && sum > 3.0_f64
        && sum >= 3.75_f64
        && quotient < 4.0_f64
        && quotient <= 3.0_f64
        && signed_zero == 0.0_f64
        && nan != nan
}

fn main() {
    println!(
        "{}",
        if strict_f32_check() && strict_f64_check() {
            "Float strict ok"
        } else {
            "Float strict bad"
        }
    );
}
