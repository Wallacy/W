// Expected exit: 0
// Expected stdout:
// 0,4,8

use std::hint::black_box;

fn scan(limit: i64) -> i64 {
    let mut index = 0;
    let mut total = 0;
    while index < limit {
        index += 1;
        if index == 2 { continue; }
        if index == 5 { break; }
        total += index;
    }
    total
}

fn main() {
    let [first, second, third] = black_box([0_i64, 3, 9]);
    println!("{},{},{}", scan(first), scan(second), scan(third));
}
