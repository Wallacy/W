// Rust 2024 reference for the Restaurant enum-subset executable workload.

#[allow(dead_code)]
#[derive(Clone, Copy)]
enum ServiceStage {
    Accepted,
    Reserving,
    Preparing,
    Serving,
    Completed,
}

// Rust has no type-level enum subsets; this alias carries the WorkStage contract.
type WorkStage = ServiceStage;

fn work_value(stage: WorkStage) -> i64 {
    match stage {
        ServiceStage::Preparing => 1,
        ServiceStage::Serving => 2,
        _ => 0,
    }
}

fn main() {
    let preparing: WorkStage = ServiceStage::Preparing;
    let serving: WorkStage = ServiceStage::Serving;
    println!("Work {}/{}", work_value(preparing), work_value(serving));
}
