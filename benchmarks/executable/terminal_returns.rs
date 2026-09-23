// Expected exit: 0
// Expected stdout:
// -1,0,1

use std::hint::black_box;

fn sign(value: i32) -> i32 {
    if value < 0 {
        return -1;
    } else if value == 0 {
        return 0;
    }
    1
}

fn main() {
    let [negative_input, zero_input, positive_input] = black_box([-5_i32, 0, 7]);
    let negative = sign(negative_input);
    let zero = sign(zero_input);
    let positive = sign(positive_input);
    println!("{negative},{zero},{positive}");
}
