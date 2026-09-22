// Expected with 0 user arguments: exit 0, stdout <empty>, stderr <empty>.
// Expected with 128 user arguments: exit 1, stdout <empty>, stderr <empty>.

fn main() {
    let count = std::env::args_os().count().saturating_sub(1);
    if i8::try_from(count).is_err() {
        std::process::exit(1);
    }
}
