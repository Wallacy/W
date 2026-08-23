// atlas:begin operators
module atlas_operators

fn operatorSurface() {
  var value = 8
  var other = 2
  var fallback = 1
  var bits = 0
  var registers = [0, 1]

  value = other
  value += other
  value -= other
  value *= other
  value /= other
  value %= other
  value **= other
  value <<= other
  value >>= other
  value &= other
  value ^= other
  value |= other
  registers[index()] <<= 1

  let coalesced = optionalValue ?? fallback
  let logical = ready || pending && enabled
  let bitwise = value | other ^ fallback & bits
  let equal = value == other
  let related = value < other
  let typed = value is Int
  let contained = value in 0..<other
  let ranges = 0...other
  // Bounded and one-sided forms include `...` and `..<`.
  // `>..` and `>..<` are current contract spellings with a Tree-sitter witness gap.
  let shifts = value << other >> bits
  let arithmetic = value + other - bits
  let products = value * other / fallback % other @ other
  let power = -2 ** 2
  let chained = source?.field
  let propagated = optionalValue?
  let selected = select(..<other)
  let selectedUpper = select(value...)
  let named = u16.checkedAdd(value, other)
  let checkedSubtract = u16.checkedSubtract(value, other)
  let checkedMultiply = u16.checkedMultiply(value, other)
  let checkedNegate = u16.checkedNegate(value)
  let checkedDivide = u16.checkedDivide(value, other)
  let checkedPower = u16.checkedPower(value, other)
  let checkedShiftLeft = u16.checkedShiftLeft(value, other)
  let checkedShiftRight = u16.checkedShiftRight(value, other)
  let wrapped = u16.wrappingAdd(value, other)
  let wrappingShiftLeft = u16.wrappingShiftLeft(value, other)
  let clipped = u16.saturatingAdd(value, other)
  let flagged = u16.overflowingAdd(value, other)
  let carry = u16.carryingAdd(value, other)
  let borrow = u16.borrowingSubtract(value, other)
  let full = u16.fullMultiply(value, other)
  let shifted = u16.maskedShiftRight(value, other)
  let maskedShiftLeft = u16.maskedShiftLeft(value, other)
  let logicalShift = u16.logicalShiftRight(value, other)
  let rotated = u16.rotatedLeft(value, other)
  let rotatedRight = u16.rotatedRight(value, other)
  let encoded = value.toBits()
  let decoded = u16.fromBits(encoded)
  let wire = value.toBytes(order: .big)
  let restored = u16.fromBytes(wire, order: .big)

  let _ = coalesced
  let _ = logical
  let _ = bitwise
  let _ = equal
  let _ = related
  let _ = typed
  let _ = contained
  let _ = ranges
  let _ = shifts
  let _ = arithmetic
  let _ = products
  let _ = power
  let _ = chained
  let _ = propagated
  let _ = selected
  let _ = named
  let _ = checkedSubtract
  let _ = checkedMultiply
  let _ = checkedNegate
  let _ = checkedDivide
  let _ = checkedPower
  let _ = checkedShiftLeft
  let _ = checkedShiftRight
  let _ = wrapped
  let _ = wrappingShiftLeft
  let _ = clipped
  let _ = flagged
  let _ = carry
  let _ = borrow
  let _ = full
  let _ = shifted
  let _ = maskedShiftLeft
  let _ = logicalShift
  let _ = rotated
  let _ = rotatedRight
  let _ = decoded
  let _ = restored
}
// atlas:end operators
