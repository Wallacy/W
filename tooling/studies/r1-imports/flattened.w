// R1 Last Light import study variant.

import std.text

enum ConsoleMode {
  cli
  tui
  serve
  unknown
}

fn decodeConsoleMode(input: ref String): ConsoleMode {
  let normalized = lowercase(trim(input))
  return switch normalized {
    case "cli": .cli
    case "tui": .tui
    case "serve": .serve
    case _: .unknown
  }
}

test "flattened import decodes the console mode" for decodeConsoleMode {
  expect decodeConsoleMode(" TUI ") == .tui
  expect decodeConsoleMode("orbit") == .unknown
}
