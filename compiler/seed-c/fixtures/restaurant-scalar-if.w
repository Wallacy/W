fn serve(isOpen: Bool, openCount: i64, closedCount: i64): i64 {
  return if isOpen { openCount } else { closedCount }
}

fn main() {
  let open = serve(isOpen: true, openCount: 5, closedCount: 2)
  let closed = serve(isOpen: false, openCount: 5, closedCount: 2)
  print("Open ${open}; closed ${closed}")
}

entry(main)
