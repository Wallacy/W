fn serve(isOpen: Bool, guests: i64): i64 {
  return if isOpen { guests + 1 } else { guests - 1 }
}

entry {
  let open = serve(isOpen: true, guests: 5)
  let closed = serve(isOpen: false, guests: 2)
  print("Open ${open}; closed ${closed}")
}
