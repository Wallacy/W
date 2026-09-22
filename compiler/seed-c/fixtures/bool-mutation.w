fn availability(requested: Bool): Bool {
  var open = false
  open = requested
  return open
}

entry {
  let open = availability(requested: true)
  let closed = availability(requested: false)
  print("Open ${open}; closed ${closed}")
}
