// Rust 2024 correctness reference for checked ordinary shifts and named fixed-width policies.
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
// i8 -128/0/-64/64
// u8 128/0/64/64
// i16 -32768/0/-16384/16384
// u16 32768/0/16384/16384
// i32 -2147483648/0/-1073741824/1073741824
// u32 2147483648/0/1073741824/1073741824
// i64 -9223372036854775808/0/-4611686018427387904/4611686018427387904
// u64 9223372036854775808/0/4611686018427387904/4611686018427387904
// u64 small-value 2/64/64

use std::hint::black_box;
use std::io::{self, BufWriter, Write};

type Int = isize;
type UInt = usize;

const _: [(); 8] = [(); std::mem::size_of::<Int>()];
const _: [(); 8] = [(); std::mem::size_of::<UInt>()];

macro_rules! checked_signed_row {
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

macro_rules! checked_unsigned_row {
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

macro_rules! policy_signed_row {
    ($out:expr, $label:literal, $signed:ty, $unsigned:ty) => {{
        let value = black_box(<$signed>::MIN);
        let width = <$signed>::BITS;
        let identity = value.wrapping_shl(black_box(width));
        let wrapped = value.wrapping_shl(black_box(width + 1));
        let arithmetic = value.wrapping_shr(black_box(width + 1));
        let logical_zero = ((value as $unsigned) >> black_box(0)) as $signed;
        let logical_one = ((value as $unsigned) >> black_box(1)) as $signed;
        let logical_max = ((value as $unsigned) >> black_box(width - 1)) as $signed;
        assert_eq!(logical_zero, value);
        assert_eq!(logical_max, 1 as $signed);
        writeln!($out, "{} {}/{}/{}/{}", $label, identity, wrapped, arithmetic, logical_one)
    }};
}

macro_rules! policy_unsigned_row {
    ($out:expr, $label:literal, $unsigned:ty) => {{
        let width = <$unsigned>::BITS;
        let value: $unsigned = black_box((1 as $unsigned) << (width - 1));
        let identity = value.wrapping_shl(black_box(width));
        let wrapped = value.wrapping_shl(black_box(width + 1));
        let arithmetic = value.wrapping_shr(black_box(width + 1));
        let logical_zero = value >> black_box(0);
        let logical_one = value >> black_box(1);
        let logical_max = value >> black_box(width - 1);
        assert_eq!(logical_zero, value);
        assert_eq!(logical_max, 1 as $unsigned);
        writeln!($out, "{} {}/{}/{}/{}", $label, identity, wrapped, arithmetic, logical_one)
    }};
}

fn main() -> io::Result<()> {
    let stdout = io::stdout();
    let mut output = BufWriter::new(stdout.lock());
    checked_signed_row!(&mut output, "i8", i8, -64_i8, -64_i8)?;
    checked_unsigned_row!(&mut output, "u8", u8, 128_u8, 64_u8)?;
    checked_signed_row!(&mut output, "i16", i16, -16_384_i16, -16_384_i16)?;
    checked_unsigned_row!(&mut output, "u16", u16, 32_768_u16, 16_384_u16)?;
    checked_signed_row!(&mut output, "i32", i32, -1_073_741_824_i32, -1_073_741_824_i32)?;
    checked_unsigned_row!(&mut output, "u32", u32, 2_147_483_648_u32, 1_073_741_824_u32)?;
    checked_signed_row!(
        &mut output,
        "i64",
        i64,
        -4_611_686_018_427_387_904_i64,
        -4_611_686_018_427_387_904_i64,
    )?;
    checked_unsigned_row!(
        &mut output,
        "u64",
        u64,
        9_223_372_036_854_775_808_u64,
        4_611_686_018_427_387_904_u64,
    )?;
    checked_signed_row!(
        &mut output,
        "Int",
        Int,
        -4_611_686_018_427_387_904_isize,
        -4_611_686_018_427_387_904_isize,
    )?;
    checked_unsigned_row!(
        &mut output,
        "UInt",
        UInt,
        9_223_372_036_854_775_808_usize,
        4_611_686_018_427_387_904_usize,
    )?;

    policy_signed_row!(&mut output, "i8", i8, u8)?;
    policy_unsigned_row!(&mut output, "u8", u8)?;
    policy_signed_row!(&mut output, "i16", i16, u16)?;
    policy_unsigned_row!(&mut output, "u16", u16)?;
    policy_signed_row!(&mut output, "i32", i32, u32)?;
    policy_unsigned_row!(&mut output, "u32", u32)?;
    policy_signed_row!(&mut output, "i64", i64, u64)?;
    policy_unsigned_row!(&mut output, "u64", u64)?;

    let small_left = black_box(1_u64).wrapping_shl(black_box(65));
    let small_right = black_box(128_u64).wrapping_shr(black_box(65));
    let small_logical = black_box(128_u64) >> black_box(1);
    writeln!(output, "u64 small-value {}/{}/{}", small_left, small_right, small_logical)?;
    output.flush()
}
