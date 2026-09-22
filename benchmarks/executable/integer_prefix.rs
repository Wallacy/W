// Rust 2024 reference for the fixed-width integer prefix family.
// Expected exit: 0
// Expected stdout:
// i8 -7/-43
// i16 -7/-43
// i32 -7/-43
// i64 -7/-43
// Int -7/-43
// u8 170
// u16 65450
// u32 4294967210
// u64 18446744073709551530
// UInt 18446744073709551530
// literal -7

use std::hint::black_box;

type Int = i64;
type UInt = u64;

fn main() {
    let signed8 = black_box(7_i8);
    let unsigned8 = black_box(0x55_u8);
    let bits8 = black_box(42_i8);
    let signed16 = black_box(7_i16);
    let unsigned16 = black_box(0x55_u16);
    let bits16 = black_box(42_i16);
    let signed32 = black_box(7_i32);
    let unsigned32 = black_box(0x55_u32);
    let bits32 = black_box(42_i32);
    let signed64 = black_box(7_i64);
    let unsigned64 = black_box(0x55_u64);
    let bits64 = black_box(42_i64);
    let int_value: Int = black_box(7_i64);
    let uint_value: UInt = black_box(0x55_u64);
    let int_bits: Int = black_box(42_i64);

    println!("i8 {}/{}", -signed8, !bits8);
    println!("i16 {}/{}", -signed16, !bits16);
    println!("i32 {}/{}", -signed32, !bits32);
    println!("i64 {}/{}", -signed64, !bits64);
    println!("Int {}/{}", -int_value, !int_bits);
    println!("u8 {}", !unsigned8);
    println!("u16 {}", !unsigned16);
    println!("u32 {}", !unsigned32);
    println!("u64 {}", !unsigned64);
    println!("UInt {}", !uint_value);
    println!("literal -7");
}
