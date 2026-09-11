fn portion(total: i64, among guests: i64): i64 {
  return total / guests
}

fn leftovers(total: i64, among guests: i64): i64 {
  return total % guests
}

entry {
  let each = portion(total: 23, among: 3)
  let remaining = leftovers(total: 23, among: 3)
  print("Each ${each}; left ${remaining}")
}
