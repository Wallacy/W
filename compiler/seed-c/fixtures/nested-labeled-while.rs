// Expected: exit 0; stdout: "0,1,3\n"; stderr: ""
fn walk(limit: i64) -> i64 {
    let mut outer = 0;
    let mut inner: i64;
    let mut total = 0;
    'outer_loop: while outer < limit {
        outer += 1;
        inner = 0;
        while inner < limit {
            inner += 1;
            if inner == 2 {
                continue;
            }
            if inner == 3 {
                break;
            }
            if outer == 4 {
                continue 'outer_loop;
            }
            if outer == 5 {
                break 'outer_loop;
            }
            total += 1;
        }
    }
    total
}

fn main() {
    let zero = walk(0);
    let one = walk(1);
    let six = walk(6);
    println!("{zero},{one},{six}");
}
