// Rust 2024 source variant for the Restaurant enum-payload workload.

enum Course {
    Starter,
    Main { price: i64, tax: i64 },
    Dessert { price: i64 },
}

fn order(price: i64, tax: i64) -> Course {
    Course::Main { tax, price }
}

fn bill(course: Course) -> i64 {
    match course {
        Course::Dessert { price: amount } => amount,
        Course::Starter => 10,
        Course::Main { tax: fee, price: amount } => amount + fee,
    }
}

fn main() {
    let first = order(30, 2);
    let second = order(41, 3);
    let starter = Course::Starter;
    let dessert = Course::Dessert { price: 7 };
    println!(
        "Bills {}/{}/{}/{}",
        bill(first),
        bill(second),
        bill(starter),
        bill(dessert)
    );
}
