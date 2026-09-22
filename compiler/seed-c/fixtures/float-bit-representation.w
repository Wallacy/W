// Expected exit: 0
// Expected stdout:
// Float bits f32 2147483648/2139095040/2143363909 f64 9223372036854775808/9218868437227405312/9221140253039434428
entry {
  let f32NegativeZero: f32 = f32.fromBits(0x80000000_u32)
  let f32Infinity: f32 = f32.fromBits(0x7f800000_u32)
  let f32Nan: f32 = f32.fromBits(0x7fc12345_u32)
  let f32NegativeZeroBits: u32 = f32NegativeZero.toBits()
  let f32InfinityBits: u32 = f32Infinity.toBits()
  let f32NanBits: u32 = f32Nan.toBits()
  let f64NegativeZero: f64 = f64.fromBits(0x8000000000000000_u64)
  let f64Infinity: f64 = f64.fromBits(0x7ff0000000000000_u64)
  let f64Nan: f64 = f64.fromBits(0x7ff8123456789abc_u64)
  let f64NegativeZeroBits: u64 = f64NegativeZero.toBits()
  let f64InfinityBits: u64 = f64Infinity.toBits()
  let f64NanBits: u64 = f64Nan.toBits()

  print("Float bits f32 ${f32NegativeZeroBits}/${f32InfinityBits}/${f32NanBits} f64 ${f64NegativeZeroBits}/${f64InfinityBits}/${f64NanBits}")
}
