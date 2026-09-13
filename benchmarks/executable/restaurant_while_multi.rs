fn serve(limit: i64) -> i64 {
    let mut served = 0;
    let mut total = 0;
    while served < limit {
        total += 2;
        served += 1;
    }
    served + total
}

fn main() {
    println!("Served {}", serve(3));
}
