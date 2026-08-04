// R1 Last Light control-flow study variant.

struct DiagnosticFold {
  bits: u32
  finalizedRows: u32
}

fn foldDiagnosticBits(rows: ref Array<Array<u8>>): DiagnosticFold {
  var bits: u32 = 0
  var finalizedRows: u32 = 0

  assembleWord: {
    scanRows: for ref row in rows {
      for value in row {
        if value == 0 { continue scanRows }
        if value > 31 { break assembleWord }
        bits <<= 5
        bits |= value
      }

      finalizedRows += 1
    }
  }

  return DiagnosticFold(bits: bits, finalizedRows: finalizedRows)
}

test "control variant preserves the diagnostic outcome" for foldDiagnosticBits {
  let result = foldDiagnosticBits([
    [1, 2, 0, 31],
    [3, 4],
    [32, 5],
  ])

  expect result.bits == 0x0000_8864
  expect result.finalizedRows == 1
}
