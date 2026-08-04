// R1 Last Light import study variant.

import text from std

enum ConsoleMode {
  cli
  tui
  serve
  unknown
}

fn decodeConsoleMode(input: ref String): ConsoleMode {
  let normalized = text.lowercase(text.trim(input))
  return switch normalized {
    case "cli": .cli
    case "tui": .tui
    case "serve": .serve
    case _: .unknown
  }
}

test "qualified import decodes the console mode" for decodeConsoleMode {
  expect decodeConsoleMode(" TUI ") == .tui
  expect decodeConsoleMode("orbit") == .unknown
}
