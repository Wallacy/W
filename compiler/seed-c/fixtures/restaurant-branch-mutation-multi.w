fn nextState(isOpen: Bool): i64 {
  var seats = 5
  var tables = 2
  if isOpen {
    tables = tables + 10
    seats = seats + 1
  } else {
    seats = seats - 1
    tables = tables - 10
  }
  return seats + tables
}

entry {
  let open = nextState(isOpen: true)
  let closed = nextState(isOpen: false)
  print("Open ${open}; closed ${closed}")
}
