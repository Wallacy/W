// BMD3 source-shaped learner profile. Backend and runtime remain unavailable.

struct ByteScanResult {
  bytes: u64
  matches: u64
}

export fn byteScanView(source: view Bytes, delimiter: u8): ByteScanResult {
  var index: usize = 0
  var matches: u64 = 0

  while index < source.count {
    let byte = source[index]
    if byte == delimiter { matches += 1 }
    index += 1
  }

  return ByteScanResult(
    bytes: u64(source.count),
    matches: matches,
  )
}

