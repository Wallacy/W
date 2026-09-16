// Rust 2024 reference for the Restaurant strict binary64 arithmetic workload.

use std::hint::black_box;

fn strict_float_check() -> bool {
    let sum = black_box(1.5_f64) + black_box(2.25_f64);
    let difference = black_box(9.5_f64) - black_box(5.5_f64);
    let product = black_box(1.5_f64) * black_box(2.0_f64);
    let quotient = black_box(7.5_f64) / black_box(2.5_f64);
    let signed_zero = -black_box(0.0_f64);
    let nan = black_box(0.0_f64) / black_box(0.0_f64);

    sum == 3.75 &&
        difference == 4.0 &&
        product == 3.0 &&
        quotient != 4.0 &&
        sum > 3.0 &&
        sum >= 3.75 &&
        quotient < 4.0 &&
        quotient <= 3.0 &&
        signed_zero == 0.0 &&
        nan != nan
}

fn main() {
    if strict_float_check() {
        println!("Float strict ok");
    } else {
        println!("Float strict bad");
    }
}
