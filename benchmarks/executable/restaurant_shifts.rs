// Rust 2024 correctness reference for checked ordinary integer shifts.
// Expected exit: 0
// Expected stdout:
// i8 -16/-128
// u8 32/128
// i16 -4096/-32768
// u16 8192/32768
// i32 -268435456/-2147483648
// u32 536870912/2147483648
// i64 -1152921504606846976/-9223372036854775808
// u64 2305843009213693952/9223372036854775808
// Int -1152921504606846976/-9223372036854775808
// UInt 2305843009213693952/9223372036854775808

use std::hint::black_box;
use std::io::{self, BufWriter, Write};

type Int = isize;
type UInt = usize;

const _: [(); 8] = [(); std::mem::size_of::<Int>()];
const _: [(); 8] = [(); std::mem::size_of::<UInt>()];

macro_rules! signed_row {
    ($output:expr, $label:literal, $type:ty, $right:expr, $left:expr $(,)?) => {{
        let right_value: $type = black_box($right);
        let left_value: $type = black_box($left);
        let right_count = black_box(2_u32);
        let left_count = black_box(1_u32);
        if right_count >= <$type>::BITS || left_count >= <$type>::BITS {
            return Err(io::Error::new(io::ErrorKind::InvalidInput, "shift count out of range"));
        }
        let left_factor: $type = (1 as $type)
            .checked_shl(left_count)
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "shift count out of range"))?;
        let right_result = right_value >> right_count;
        let left_result = left_value
            .checked_mul(left_factor)
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "signed left shift overflow"))?;
        writeln!($output, "{} {}/{}", $label, right_result, left_result)
    }};
}

macro_rules! unsigned_row {
    ($output:expr, $label:literal, $type:ty, $right:expr, $left:expr $(,)?) => {{
        let right_value: $type = black_box($right);
        let left_value: $type = black_box($left);
        let right_count = black_box(2_u32);
        let left_count = black_box(1_u32);
        if right_count >= <$type>::BITS || left_count >= <$type>::BITS {
            return Err(io::Error::new(io::ErrorKind::InvalidInput, "shift count out of range"));
        }
        let left_factor: $type = (1 as $type)
            .checked_shl(left_count)
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "shift count out of range"))?;
        let right_result = right_value >> right_count;
        let left_result = left_value
            .checked_mul(left_factor)
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "unsigned left shift overflow"))?;
        writeln!($output, "{} {}/{}", $label, right_result, left_result)
    }};
}

fn main() -> io::Result<()> {
    let stdout = io::stdout();
    let mut output = BufWriter::new(stdout.lock());
    signed_row!(&mut output, "i8", i8, -64_i8, -64_i8)?;
    unsigned_row!(&mut output, "u8", u8, 128_u8, 64_u8)?;
    signed_row!(&mut output, "i16", i16, -16_384_i16, -16_384_i16)?;
    unsigned_row!(&mut output, "u16", u16, 32_768_u16, 16_384_u16)?;
    signed_row!(&mut output, "i32", i32, -1_073_741_824_i32, -1_073_741_824_i32)?;
    unsigned_row!(&mut output, "u32", u32, 2_147_483_648_u32, 1_073_741_824_u32)?;
    signed_row!(
        &mut output,
        "i64",
        i64,
        -4_611_686_018_427_387_904_i64,
        -4_611_686_018_427_387_904_i64,
    )?;
    unsigned_row!(
        &mut output,
        "u64",
        u64,
        9_223_372_036_854_775_808_u64,
        4_611_686_018_427_387_904_u64,
    )?;
    signed_row!(
        &mut output,
        "Int",
        Int,
        -4_611_686_018_427_387_904_isize,
        -4_611_686_018_427_387_904_isize,
    )?;
    unsigned_row!(
        &mut output,
        "UInt",
        UInt,
        9_223_372_036_854_775_808_usize,
        4_611_686_018_427_387_904_usize,
    )?;
    output.flush()
}
