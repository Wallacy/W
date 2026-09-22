fn fits(guests: i64, seats: i64): Bool {
  print("Capacity checked")
  return guests <= seats
}
fn overrideAllowed(blocked: Bool): Bool {
  print("Override checked")
  return !blocked
}
fn allowed(isOpen: Bool, guests: i64, seats: i64, blocked: Bool): Bool {
  return (isOpen && fits(guests: guests, seats: seats)) ||
    overrideAllowed(blocked: blocked)
}
fn main() {
  let closed = allowed(isOpen: false, guests: 5, seats: 4, blocked: false)
  print("Closed allowed ${closed}")
  let open = allowed(isOpen: true, guests: 3, seats: 4, blocked: true)
  print("Open allowed ${open}")
}
entry(main)
