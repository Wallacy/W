// Rust 2024 reference for the consolidated Restaurant UInt bit-primitives family.
// Expected exit: 0
// Expected stdout:
// Not 18446744073709551615
// And 0
// Or 18446744073709551615
// Xor 18446744073709551615
// Ones 32
// Zeros 32
// Leading 56
// Leading zero 64
// Trailing 12
// Trailing zero 64

use std::hint::black_box;
use std::io::{self, Write};

fn main() {
    let zero = black_box(0_u64);
    let high = black_box(9_223_372_036_854_775_808_u64);
    let lower = black_box(9_223_372_036_854_775_807_u64);
    let population_input = black_box(0xf0f0_f0f0_0f0f_0f0f_u64);
    let leading_input = black_box(0x0000_0000_0000_00f0_u64);
    let trailing_input = black_box(0x0000_0000_0000_f000_u64);
    let bit_not = !zero;
    let bit_and = high & lower;
    let bit_or = high | lower;
    let bit_xor = high ^ lower;
    let ones = population_input.count_ones();
    let zeros = population_input.count_zeros();
    let leading = leading_input.leading_zeros();
    let leading_zero = zero.leading_zeros();
    let trailing = trailing_input.trailing_zeros();
    let trailing_zero = zero.trailing_zeros();
    let mut stdout = io::stdout().lock();
    if writeln!(
        stdout,
        concat!(
            "Not {}\nAnd {}\nOr {}\nXor {}\nOnes {}\nZeros {}\n",
            "Leading {}\nLeading zero {}\nTrailing {}\nTrailing zero {}",
        ),
        bit_not,
        bit_and,
        bit_or,
        bit_xor,
        ones,
        zeros,
        leading,
        leading_zero,
        trailing,
        trailing_zero,
    )
    .is_err()
    {
        std::process::exit(1);
    }
}
