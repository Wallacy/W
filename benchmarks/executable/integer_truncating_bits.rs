// Expected exit: 0
// Expected stdout:
// Trunc 2/-7/-6/18446744073709551609/-1

use std::hint::black_box;

fn main() {
    let signed_narrow = black_box(258_i16) as i8;
    let signed_widen = black_box(-7_i8) as i16;
    let unsigned_narrow = black_box(250_u8) as i8;
    let signed_alias_to_unsigned = black_box(-7_i64) as u64;
    let unsigned_alias_to_signed = black_box(u64::MAX) as i64;
    println!(
        "Trunc {signed_narrow}/{signed_widen}/{unsigned_narrow}/{signed_alias_to_unsigned}/{unsigned_alias_to_signed}"
    );
}
