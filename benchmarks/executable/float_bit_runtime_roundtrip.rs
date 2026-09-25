// Rust 2024 correctness reference for a runtime-derived f64/u64 bit round trip.
// Expected exit: 0
// Expected stdout:
// Bits 0
// Expected stderr:

fn main() {
    let bits = std::env::args_os().skip(1).count() as u64;
    let roundtrip = f64::from_bits(bits).to_bits();
    println!("Bits {roundtrip}");
}
