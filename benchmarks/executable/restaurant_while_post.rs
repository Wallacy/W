fn settle(limit: i64) -> i64 {
    let mut served = 0;
    let mut total = 0;
    while served < limit {
        total += 2;
        served += 1;
    }
    total += served;
    total
}

fn main() {
    println!("Final {}", settle(3));
}
