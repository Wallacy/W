fn admit(guests: i64, seats: i64) {
  if guests <= seats { print("Seat party") } else { print("Waitlist") }
}
fn main() {
  admit(guests: 3, seats: 4)
  admit(guests: 4, seats: 4)
  admit(guests: 5, seats: 4)
}
entry(main)
