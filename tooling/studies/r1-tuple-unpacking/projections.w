// R1 Last Light tuple-unpacking study variant.

struct MenuCompileError {
  let line: usize
}

fn word(): (String, usize) throws MenuCompileError {
  return ("horizon", 7)
}

fn readWord(): (String, usize) throws MenuCompileError {
  let result = try word()
  let text = copy result.0
  let foundLine = result.1
  return (text, foundLine)
}

test "tuple projections evaluate word once" for readWord {
  expect try readWord() == ("horizon", 7)
}
