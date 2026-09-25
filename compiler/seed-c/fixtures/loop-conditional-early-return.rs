// Expected: exit 0; stdout: "13,2,0\n"; stderr: ""
fn first_at(limit: i64) -> i64 {
    let mut index: i64 = 0;
    while index < limit {
        index += 1;
        if index == 3 {
            return index + 10;
        }
    }
    index
}

fn main() {
    let early = first_at(5);
    let exhausted = first_at(2);
    let empty = first_at(0);
    println!("{early},{exhausted},{empty}");
}
