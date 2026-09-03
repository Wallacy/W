// R1 Last Light tuple-unpacking study variant.

struct MenuCompileError {
  let line: usize
}

fn word(): (String, usize) throws MenuCompileError {
  return ("horizon", 7)
}

fn readWord(): (String, usize) throws MenuCompileError {
  let (text, foundLine) = try word()
  return (text, foundLine)
}

test "tuple binding evaluates word once" for readWord {
  expect try readWord() == ("horizon", 7)
}
