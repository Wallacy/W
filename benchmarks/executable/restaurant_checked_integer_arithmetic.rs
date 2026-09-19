// Rust 2024 reference for checked fixed-width integer arithmetic.
// Expected exit: 0
// Expected stdout:
// i8 -9/-15/-36; divrem -4/0; compound -2
// u8 43/37/120; divrem 13/1; compound 2
// i16 -970/-1030/-30000; divrem -33/-10; compound -12
// u16 1030/970/30000; divrem 33/10; compound 8
// i32 -117000/-123000/-360000000; divrem -40/0; compound -2
// u32 100300/99700/30000000; divrem 333/100; compound 98
// i64 -600000/-1200000/-270000000000; divrem -3/0; compound -2
// u64 6000000000/4000000000/5000000000000000000; divrem 5/0; compound 999999998
// Int -4000000000/-6000000000/-5000000000000000000; divrem -5/0; compound -2
// UInt 9000000000/3000000000/18000000000000000000; divrem 2/0; compound 2999999998

use std::hint::black_box;

type Int = i64;
type UInt = u64;

macro_rules! report_signed {
    ($label:literal, $type:ty, $left:expr, $right:expr) => {{
        let (mut left, right) = black_box(($left as $type, $right as $type));
        let sum = left + right;
        let difference = left - right;
        let product = left * right;
        let quotient = left / right;
        let remainder = left % right;
        left += right;
        left -= 2 as $type;
        left *= 2 as $type;
        left /= 2 as $type;
        left %= right;
        println!(
            "{} {}/{}/{}; divrem {}/{}; compound {}",
            $label, sum, difference, product, quotient, remainder, left
        );
    }};
}

macro_rules! report_unsigned {
    ($label:literal, $type:ty, $left:expr, $right:expr) => {{
        let (mut left, right) = black_box(($left as $type, $right as $type));
        let sum = left + right;
        let difference = left - right;
        let product = left * right;
        let quotient = left / right;
        let remainder = left % right;
        left += right;
        left -= 2 as $type;
        left *= 2 as $type;
        left /= 2 as $type;
        left %= right;
        println!(
            "{} {}/{}/{}; divrem {}/{}; compound {}",
            $label, sum, difference, product, quotient, remainder, left
        );
    }};
}

fn main() {
    report_signed!("i8", i8, -12, 3);
    report_unsigned!("u8", u8, 40, 3);
    report_signed!("i16", i16, -1000, 30);
    report_unsigned!("u16", u16, 1000, 30);
    report_signed!("i32", i32, -120000, 3000);
    report_unsigned!("u32", u32, 100000, 300);
    report_signed!("i64", i64, -900000, 300000);
    report_unsigned!("u64", u64, 5_000_000_000, 1_000_000_000);
    report_signed!("Int", Int, -5_000_000_000_i64, 1_000_000_000_i64);
    report_unsigned!("UInt", UInt, 6_000_000_000_u64, 3_000_000_000_u64);
}
