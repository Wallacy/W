// Rust 2024 source variant for the Restaurant virtual structured-task elision workload.

fn prepare(value: i64) -> i64 {
    value
}

fn main() {
    let first = prepare(20);
    let second = prepare(22);
    println!("Prepared {}", first + second);
}
