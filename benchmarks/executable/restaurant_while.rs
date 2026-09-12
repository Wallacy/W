fn count_to(limit: i64) -> i64 {
    let mut count = 0;
    while count < limit {
        count += 1;
    }
    count
}

fn main() {
    println!("Served {}", count_to(3));
}
