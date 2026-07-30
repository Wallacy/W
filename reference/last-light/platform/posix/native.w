// POSIX implementation selected by the package target graph.

import { NativeTerminalBackend } from restaurant.platform

export fn nativeTerminalBackend(): NativeTerminalBackend {
  return .ansi
}
