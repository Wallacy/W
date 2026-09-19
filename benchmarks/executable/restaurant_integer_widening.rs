// Expected exit: 0
// Expected stdout:
// Widen -7/200/202/203

use std::hint::black_box;

fn main() {
    let returned: i16 = black_box(-7_i8).into();
    let called: i16 = black_box(200_u8).into();
    let unsigned: u16 = black_box(202_u8).into();
    let alias: i64 = black_box(203_u8).into();
    println!("Widen {returned}/{called}/{unsigned}/{alias}");
}
