// Expected cases (user arguments => exit; stdout; stderr):
// [] => 0; "Rounded 42\n"; ""
// ["x"] => 1; ""; ""
// ["x", "y"] => 1; ""; ""

fn main() {
    let bits = if std::env::args_os().skip(1).next().is_none() {
        0x4045_6000_0000_0000_u64
    } else {
        0x7ff8_0000_0000_0000_u64
    };
    let value = f64::from_bits(bits);
    if !value.is_finite() || !(-128.0..128.0).contains(&value) {
        std::process::exit(1);
    }
    let rounded = value as i8;
    println!("Rounded {rounded}");
}
