// Canonical formatter fixture for source order, comments and multiline calls.

import { Command, Result } from command

export struct FormatCase {
  value: String
  expected: String
}

export fn oneLine(value: String): String { return value }

export fn namedCall(
  command: Command,
  expected: String,
): FormatCase {
  let value = command.label()
  return FormatCase(value: value, expected: expected)
}

test "formatter fixture preserves declaration order" for oneLine {
  let value = oneLine(value: "Last Light")
  expect value == "Last Light"
}
