// BMD3 source-shaped frontier profile. This open lane exposes a SIMD strategy.
// The scalar tail preserves the same logical contract and input.

import { Simd } from std.simd

struct ByteScanResult {
  let bytes: u64
  let matches: u64
}

export fn byteScanView(source: view Bytes, delimiter: u8): ByteScanResult {
  let delimiterVector = Simd<u8, lanes: 16>.splat(delimiter)
  var offset: usize = 0
  var matches: u64 = 0

  while offset + 16 <= source.count {
    let chunk = Simd<u8, lanes: 16>.load(from: source, at: offset)
    matches += u64(chunk.equalLanes(delimiterVector).countTrue())
    offset += 16
  }

  if offset < source.count {
    let (tail, live) = Simd<u8, lanes: 16>.loadPartial(
      from: source,
      at: offset,
      fill: delimiter,
    )
    matches += u64((tail.equalLanes(delimiterVector) & live).countTrue())
  }

  return ByteScanResult(
    bytes: u64(source.count),
    matches: matches,
  )
}

