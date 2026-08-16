import std
// security feature witness

export fn choosePath(enabled: Bool): String {
  return if enabled { "bounded" } else { "fallback" }
}
