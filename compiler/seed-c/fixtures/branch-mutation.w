fn nextSeats(isOpen: Bool): i64 {
  var seats = 5
  if isOpen {
    seats = seats + 1
  } else {
    seats = seats - 1
  }
  return seats
}

entry {
  let open = nextSeats(isOpen: true)
  let closed = nextSeats(isOpen: false)
  print("Open ${open}; closed ${closed}")
}
