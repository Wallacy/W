// Expected: exit 0; stdout: "7,5,26\n"; stderr: ""
#[derive(Clone, Copy)]
struct ValuePair {
    left: i64,
    right: i64,
}

fn make_tuple_pair(left: i64, right: i64) -> (i64, i64) {
    (left, right)
}

fn combine_tuple_pair(pair: (i64, i64), scale: i64) -> i64 {
    let product = pair.0 * scale;
    product + pair.1
}

fn make_value_pair(left: i64, right: i64) -> ValuePair {
    ValuePair { right, left }
}

fn combine_value_pair(pair: ValuePair, scale: i64) -> i64 {
    let product = pair.left * scale;
    product + pair.right
}

fn main() {
    let tuple = make_tuple_pair(7, 5);
    let value = make_value_pair(tuple.0, tuple.1);
    let tuple_result = combine_tuple_pair(tuple, 3);
    let value_result = combine_value_pair(value, 3);
    let result = tuple_result + value_result - 26;
    println!("{},{},{}", value.left, value.right, result);
}
