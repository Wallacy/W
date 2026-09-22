// Expected with 0 user arguments: exit 0, stdout "Exact 0\n", stderr <empty>.
// Expected with 127 user arguments: exit 0, stdout "Exact 127\n", stderr <empty>.
// Expected with 128 user arguments: exit 1, stdout <empty>, stderr <empty>.

fn main() {
    let count = std::env::args_os().count().saturating_sub(1);
    let Ok(narrowed) = i8::try_from(count) else { std::process::exit(1) };
    println!("Exact {narrowed}");
}
