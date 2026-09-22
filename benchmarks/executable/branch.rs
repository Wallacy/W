// Rust 2024 source variant for the Restaurant branch workload.

fn serve(is_open: bool) {
    if is_open {
        print!("Kitchen open\n");
    } else {
        print!("Kitchen closed\n");
    }
    print!("After service\n");
}

fn main() {
    serve(true);
    serve(false);
}
