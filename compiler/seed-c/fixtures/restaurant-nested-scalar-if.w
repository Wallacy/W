fn choose(
  outer: Bool,
  inner: Bool,
  open: i64,
  middle: i64,
  closed: i64,
): i64 {
    return if outer {
      if inner { open } else { middle }
  } else {
    closed
  }
}

fn main() {
  let first = choose(outer: true, inner: true, open: 1, middle: 2, closed: 3)
  let second = choose(outer: true, inner: false, open: 1, middle: 2, closed: 3)
  let third = choose(outer: false, inner: false, open: 1, middle: 2, closed: 3)
  print("${first},${second},${third}")
}

entry(main)
