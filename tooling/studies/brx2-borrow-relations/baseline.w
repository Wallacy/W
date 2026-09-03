// BRX2 selected fixture. The fixture is not a compiler implementation.
import std.stream

export fn observeBorrowRelation(
  _ primary: ref String,
  _ fallback: ref String,
): view String {
  return selectPrimary(primary, fallback)
}
