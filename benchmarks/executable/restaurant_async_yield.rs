// Rust 2024 sequential reference for the Restaurant static-yield lifecycle workload.

fn prepare(value: i64) -> i64 {
    // Immediate continuation is a legal schedule for W's non-barrier yield.
    let staged = value + 1;
    staged * 2
}

fn main() {
    let first = prepare(20);
    let second = prepare(22);
    println!("Prepared {}", first + second);
}
