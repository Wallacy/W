// Expected: exit 0; stdout: "7,5,26\n"; stderr: ""
#[derive(Clone, Copy)]
struct Pair {
    left: i64,
    right: i64,
}

fn make_pair(left: i64, right: i64) -> Pair {
    let pair = Pair { right, left };
    pair
}

fn combine(pair: Pair, scale: i64) -> i64 {
    let product = pair.left * scale;
    product + pair.right
}

fn main() {
    let original = make_pair(7, 5);
    let result = combine(original, 3);
    println!("{},{},{}", original.left, original.right, result);
}
