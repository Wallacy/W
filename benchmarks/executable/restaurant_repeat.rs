fn receipt_digits(value: i64) -> i64 {
    let mut remaining = value;
    let mut digits = 0;
    loop {
        digits += 1;
        remaining /= 10;
        if remaining <= 0 {
            break;
        }
    }
    digits
}

fn main() {
    println!("Receipt digits {}/{}", receipt_digits(0), receipt_digits(42_424));
}
