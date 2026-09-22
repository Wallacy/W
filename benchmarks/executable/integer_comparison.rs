// Rust 2024 reference for Restaurant fixed-width integer comparisons.
// Expected exit: 0
// Expected stdout:
// i8 false/true/true/true/false/false
// u8 false/true/false/false/true/true
// i16 true/false/false/true/false/true
// u16 false/true/true/true/false/false
// i32 false/true/true/true/false/false
// u32 false/true/false/false/true/true
// i64 false/true/true/true/false/false
// u64 false/true/false/false/true/true
// Int false/true/true/true/false/false
// UInt true/false/false/true/false/true
// widen u8->i16 true

use std::hint::black_box;

fn report_i8(left: i8, right: i8) {
    println!(
        "i8 {}/{}/{}/{}/{}/{}",
        left == right, left != right, left < right,
        left <= right, left > right, left >= right
    );
}

fn report_u8(left: u8, right: u8) {
    println!(
        "u8 {}/{}/{}/{}/{}/{}",
        left == right, left != right, left < right,
        left <= right, left > right, left >= right
    );
}

fn report_i16(left: i16, right: i16) {
    println!(
        "i16 {}/{}/{}/{}/{}/{}",
        left == right, left != right, left < right,
        left <= right, left > right, left >= right
    );
}

fn report_u16(left: u16, right: u16) {
    println!(
        "u16 {}/{}/{}/{}/{}/{}",
        left == right, left != right, left < right,
        left <= right, left > right, left >= right
    );
}

fn report_i32(left: i32, right: i32) {
    println!(
        "i32 {}/{}/{}/{}/{}/{}",
        left == right, left != right, left < right,
        left <= right, left > right, left >= right
    );
}

fn report_u32(left: u32, right: u32) {
    println!(
        "u32 {}/{}/{}/{}/{}/{}",
        left == right, left != right, left < right,
        left <= right, left > right, left >= right
    );
}

fn report_i64(left: i64, right: i64) {
    println!(
        "i64 {}/{}/{}/{}/{}/{}",
        left == right, left != right, left < right,
        left <= right, left > right, left >= right
    );
}

fn report_u64(left: u64, right: u64) {
    println!(
        "u64 {}/{}/{}/{}/{}/{}",
        left == right, left != right, left < right,
        left <= right, left > right, left >= right
    );
}

fn report_int(left: i64, right: i64) {
    println!(
        "Int {}/{}/{}/{}/{}/{}",
        left == right, left != right, left < right,
        left <= right, left > right, left >= right
    );
}

fn report_uint(left: u64, right: u64) {
    println!(
        "UInt {}/{}/{}/{}/{}/{}",
        left == right, left != right, left < right,
        left <= right, left > right, left >= right
    );
}

fn report_widened(value: i16) {
    println!("widen u8->i16 {}", value > 199_i16);
}

fn main() {
    report_i8(black_box(-1_i8), black_box(1_i8));
    report_u8(black_box(250_u8), black_box(10_u8));
    report_i16(black_box(77_i16), black_box(77_i16));
    report_u16(black_box(3_u16), black_box(50_000_u16));
    report_i32(black_box(-120_000_i32), black_box(120_000_i32));
    report_u32(black_box(3_000_000_000_u32), black_box(1_u32));
    report_i64(black_box(-9_000_000_000_i64), black_box(9_000_000_000_i64));
    report_u64(black_box(10_u64), black_box(9_u64));
    report_int(black_box(-7_i64), black_box(12_i64));
    report_uint(black_box(7_u64), black_box(7_u64));
    report_widened(black_box(200_u8).into());
}
