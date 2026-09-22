// Rust 2024 sequential reference for the Restaurant static-yield lifecycle workload.

fn stage(value: i64) -> i64 {
    value + 1
}

fn prepare(value: i64) -> i64 {
    // Immediate continuation is a legal schedule for W's non-barrier yield.
    stage(value) * 2
}

fn main() {
    let first = prepare(20);
    let second = prepare(22);
    println!("Prepared {}", first + second);
}
