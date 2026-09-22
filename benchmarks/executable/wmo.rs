fn bill(total: i64) -> i64 {
    total + 2
}

pub fn unused_receipt(total: i64) -> i64 {
    total / 2
}

fn hidden_menu() {
    println!("Never served");
}

fn main() {
    println!("Bill {}", bill(40));
}
