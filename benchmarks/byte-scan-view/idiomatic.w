// BMD3 source-shaped idiomatic profile. Backend and runtime remain unavailable.

struct ByteScanResult {
  bytes: u64
  matches: u64
}

export fn byteScanView(source: view Bytes, delimiter: u8): ByteScanResult {
  var matches: u64 = 0

  for byte in source {
    if byte == delimiter { matches += 1 }
  }

  return ByteScanResult(
    bytes: u64(source.count),
    matches: matches,
  )
}

