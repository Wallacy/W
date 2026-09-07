fn compare(left: i64, right: i64) {
  let less = left < right
  print("${left == right}/${left != right}/${less}/${left <= right}/${left > right}/${left >= right}")
}
fn fits(guests: i64, seats: i64): Bool { return guests <= seats }
fn announce(allowed: Bool) { print("Allowed ${allowed}") }
fn main() {
  compare(left: 0 - 1, right: 0)
  compare(left: 0, right: 0)
  compare(left: 1, right: 0)
  compare(left: 0 - 9223372036854775807 - 1, right: 9223372036854775807)
  compare(left: 9223372036854775807, right: 0 - 9223372036854775807 - 1)
  let allowed = fits(guests: 3, seats: 4)
  announce(allowed: allowed)
  let denied = fits(guests: 5, seats: 4)
  announce(allowed: denied)
}
entry(main)
