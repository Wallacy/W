// Expected exit: 0
// Expected stdout:
// Numeric widen ok

fn returnF32(value: i16): f32 {
  return value
}

fn acceptF64(value: f64): f64 {
  return value
}

entry {
  let signedF32: f32 = -32767_i16
  let unsignedF32: f32 = 65535_u16
  let returnedF32 = returnF32(value: -123_i16)
  let signedF64: f64 = -2147483647_i32
  let calledF64 = acceptF64(value: 4294967295_u32)
  let widenedFloat: f64 = 1.5_f32
  let explicitFloat = f64(1.25_f32)
  let mixedInteger = 2_i32 + 0.5_f64
  let mixedFloat = 1.5_f32 + 2.25_f64
  let mixedComparison = 65535_u16 == 65535.0_f32
  let valid = signedF32 == -32767.0_f32 && unsignedF32 == 65535.0_f32 &&
    returnedF32 == -123.0_f32 && signedF64 == -2147483647.0_f64 &&
    calledF64 == 4294967295.0_f64 && widenedFloat == 1.5_f64 &&
    explicitFloat == 1.25_f64 && mixedInteger == 2.5_f64 &&
    mixedFloat == 3.75_f64 && mixedComparison

  if valid {
    print("Numeric widen ok")
  } else {
    print("Numeric widen bad")
  }
}
