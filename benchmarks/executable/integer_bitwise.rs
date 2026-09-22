// Rust 2024 reference for the Restaurant integer bitwise family.
// Expected exit: 0
// Expected stdout:
// i8 10/-81/-91
// u8 10/175/165
// i16 2570/-20561/-23131
// u16 2570/44975/42405
// i32 168430090/-1347440721/-1515870811
// u32 168430090/2947526575/2779096485
// i64 723401728380766730/-5787213827046133841/-6510615555426900571
// u64 723401728380766730/12659530246663417775/11936128518282651045
// Int 723401728380766730/-5787213827046133841/-6510615555426900571
// UInt 723401728380766730/12659530246663417775/11936128518282651045
// Widened -13
// Mixed 255

use std::hint::black_box;
use std::io::{self, Write};

fn main() {
    let i8_a = black_box(-86_i8);
    let i8_b = black_box(15_i8);
    let u8_a = black_box(170_u8);
    let u8_b = black_box(15_u8);
    let i16_a = black_box(-21_846_i16);
    let i16_b = black_box(3_855_i16);
    let u16_a = black_box(43_690_u16);
    let u16_b = black_box(3_855_u16);
    let i32_a = black_box(-1_431_655_766_i32);
    let i32_b = black_box(252_645_135_i32);
    let u32_a = black_box(2_863_311_530_u32);
    let u32_b = black_box(252_645_135_u32);
    let i64_a = black_box(-6_148_914_691_236_517_206_i64);
    let i64_b = black_box(1_085_102_592_571_150_095_i64);
    let u64_a = black_box(12_297_829_382_473_034_410_u64);
    let u64_b = black_box(1_085_102_592_571_150_095_u64);
    let int_a = black_box(-6_148_914_691_236_517_206_isize);
    let int_b = black_box(1_085_102_592_571_150_095_isize);
    let uint_a = black_box(12_297_829_382_473_034_410_usize);
    let uint_b = black_box(1_085_102_592_571_150_095_usize);
    let widened_left = black_box(-16_i8);
    let widened_right = black_box(3_i32);
    let widened = i32::from(widened_left) | widened_right;
    let mixed_left = black_box(240_u8);
    let mixed_right = black_box(15_i16);
    let mixed = i16::from(mixed_left) | mixed_right;

    let mut stdout = io::stdout().lock();
    if write!(
        stdout,
        concat!(
            "i8 {}/{}/{}\n",
            "u8 {}/{}/{}\n",
            "i16 {}/{}/{}\n",
            "u16 {}/{}/{}\n",
            "i32 {}/{}/{}\n",
            "u32 {}/{}/{}\n",
            "i64 {}/{}/{}\n",
            "u64 {}/{}/{}\n",
            "Int {}/{}/{}\n",
            "UInt {}/{}/{}\n",
            "Widened {}\n",
            "Mixed {}\n",
        ),
        i8_a & i8_b,
        i8_a | i8_b,
        i8_a ^ i8_b,
        u8_a & u8_b,
        u8_a | u8_b,
        u8_a ^ u8_b,
        i16_a & i16_b,
        i16_a | i16_b,
        i16_a ^ i16_b,
        u16_a & u16_b,
        u16_a | u16_b,
        u16_a ^ u16_b,
        i32_a & i32_b,
        i32_a | i32_b,
        i32_a ^ i32_b,
        u32_a & u32_b,
        u32_a | u32_b,
        u32_a ^ u32_b,
        i64_a & i64_b,
        i64_a | i64_b,
        i64_a ^ i64_b,
        u64_a & u64_b,
        u64_a | u64_b,
        u64_a ^ u64_b,
        int_a & int_b,
        int_a | int_b,
        int_a ^ int_b,
        uint_a & uint_b,
        uint_a | uint_b,
        uint_a ^ uint_b,
        widened,
        mixed,
    )
    .is_err()
    {
        std::process::exit(1);
    }
}
