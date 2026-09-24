// Expected: exit 0; stdout: "42/0\n"; stderr: ""
enum Lookup {
  missing
  found(value: i64)
}

fn choose(available: Bool, amount: i64): Lookup {
  return if available { .found(value: amount) } else { .missing }
}

fn score(result: Lookup): i64 {
  return switch result {
    case .found(value: let value): value + 1
    case .missing: 0
  }
}

entry {
  let present_value = choose(available: true, amount: 41)
  let present = score(result: present_value)
  let absent_value = choose(available: false, amount: 41)
  let absent = score(result: absent_value)
  print("${present}/${absent}")
}
