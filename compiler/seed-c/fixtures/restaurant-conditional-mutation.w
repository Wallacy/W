fn nextSeats(isOpen: Bool): i64 {
  var seats = 5
  let selected = if isOpen { seats + 1 } else { seats - 1 }
  seats = selected
  return seats
}

entry {
  let open = nextSeats(isOpen: true)
  let closed = nextSeats(isOpen: false)
  print("Open ${open}; closed ${closed}")
}
