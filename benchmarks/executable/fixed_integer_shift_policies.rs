// Rust 2024 correctness reference for fixed-width named shift policies.
// Expected exit: 0
// Expected stdout:
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

macro_rules! signed_row {
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

macro_rules! unsigned_row {
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
    signed_row!(&mut output, "i8", i8, u8)?;
    unsigned_row!(&mut output, "u8", u8)?;
    signed_row!(&mut output, "i16", i16, u16)?;
    unsigned_row!(&mut output, "u16", u16)?;
    signed_row!(&mut output, "i32", i32, u32)?;
    unsigned_row!(&mut output, "u32", u32)?;
    signed_row!(&mut output, "i64", i64, u64)?;
    unsigned_row!(&mut output, "u64", u64)?;
    let small_left = black_box(1_u64).wrapping_shl(black_box(65));
    let small_right = black_box(128_u64).wrapping_shr(black_box(65));
    let small_logical = black_box(128_u64) >> black_box(1);
    writeln!(output, "u64 small-value {}/{}/{}", small_left, small_right, small_logical)?;
    output.flush()
}
