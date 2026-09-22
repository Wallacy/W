// Expected with 0 user arguments: exit 0, stdout "Arithmetic 0/4\n", stderr <empty>.
// Expected with 127 user arguments: exit 0, stdout "Arithmetic 127/10\n", stderr <empty>.
// Expected with 128 user arguments: exit 1, stdout <empty>, stderr <empty>.

fn main() {
    let count = std::env::args_os().count().saturating_sub(1);
    let Ok(narrowed) = i8::try_from(count) else { std::process::exit(1) };
    let arithmetic = narrowed % 11_i8 * 2_i8 / 2_i8 + 7_i8 - 3_i8;
    println!("Arithmetic {narrowed}/{arithmetic}");
}
