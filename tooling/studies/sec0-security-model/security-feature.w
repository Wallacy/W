import std
// security feature witness

export fn choosePath(_ enabled: Bool): String {
  return if enabled { "bounded" } else { "fallback" }
}
