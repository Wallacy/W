// Expected exit: 0
// Expected stdout:
// i8 -9/-15/-36; divrem -4/0; compound -2
// u8 43/37/120; divrem 13/1; compound 2
// i16 -970/-1030/-30000; divrem -33/-10; compound -12
// u16 1030/970/30000; divrem 33/10; compound 8
// i32 -117000/-123000/-360000000; divrem -40/0; compound -2
// u32 100300/99700/30000000; divrem 333/100; compound 98
// i64 -600000/-1200000/-270000000000; divrem -3/0; compound -2
// u64 6000000000/4000000000/5000000000000000000; divrem 5/0; compound 999999998
// Int -4000000000/-6000000000/-5000000000000000000; divrem -5/0; compound -2
// UInt 9000000000/3000000000/18000000000000000000; divrem 2/0; compound 2999999998

fn exerciseI8(left: i8, right: i8) {
  var compound = left
  compound += right
  compound -= 2_i8
  compound *= 2_i8
  compound /= 2_i8
  compound %= right
  print("i8 ${left + right}/${left - right}/${left * right}; divrem ${left / right}/${left % right}; compound ${compound}")
}

fn exerciseU8(left: u8, right: u8) {
  var compound = left
  compound += right
  compound -= 2_u8
  compound *= 2_u8
  compound /= 2_u8
  compound %= right
  print("u8 ${left + right}/${left - right}/${left * right}; divrem ${left / right}/${left % right}; compound ${compound}")
}

fn exerciseI16(left: i16, right: i16) {
  var compound = left
  compound += right
  compound -= 2_i16
  compound *= 2_i16
  compound /= 2_i16
  compound %= right
  print("i16 ${left + right}/${left - right}/${left * right}; divrem ${left / right}/${left % right}; compound ${compound}")
}

fn exerciseU16(left: u16, right: u16) {
  var compound = left
  compound += right
  compound -= 2_u16
  compound *= 2_u16
  compound /= 2_u16
  compound %= right
  print("u16 ${left + right}/${left - right}/${left * right}; divrem ${left / right}/${left % right}; compound ${compound}")
}

fn exerciseI32(left: i32, right: i32) {
  var compound = left
  compound += right
  compound -= 2_i32
  compound *= 2_i32
  compound /= 2_i32
  compound %= right
  print("i32 ${left + right}/${left - right}/${left * right}; divrem ${left / right}/${left % right}; compound ${compound}")
}

fn exerciseU32(left: u32, right: u32) {
  var compound = left
  compound += right
  compound -= 2_u32
  compound *= 2_u32
  compound /= 2_u32
  compound %= right
  print("u32 ${left + right}/${left - right}/${left * right}; divrem ${left / right}/${left % right}; compound ${compound}")
}

fn exerciseI64(left: i64, right: i64) {
  var compound = left
  compound += right
  compound -= 2_i64
  compound *= 2_i64
  compound /= 2_i64
  compound %= right
  print("i64 ${left + right}/${left - right}/${left * right}; divrem ${left / right}/${left % right}; compound ${compound}")
}

fn exerciseU64(left: u64, right: u64) {
  var compound = left
  compound += right
  compound -= 2_u64
  compound *= 2_u64
  compound /= 2_u64
  compound %= right
  print("u64 ${left + right}/${left - right}/${left * right}; divrem ${left / right}/${left % right}; compound ${compound}")
}

fn exerciseInt(left: Int, right: Int) {
  var compound = left
  compound += right
  compound -= 2_i64
  compound *= 2_i64
  compound /= 2_i64
  compound %= right
  print("Int ${left + right}/${left - right}/${left * right}; divrem ${left / right}/${left % right}; compound ${compound}")
}

fn exerciseUInt(left: UInt, right: UInt) {
  var compound = left
  compound += right
  compound -= 2_u64
  compound *= 2_u64
  compound /= 2_u64
  compound %= right
  print("UInt ${left + right}/${left - right}/${left * right}; divrem ${left / right}/${left % right}; compound ${compound}")
}

entry {
  exerciseI8(left: -12_i8, right: 3_i8)
  exerciseU8(left: 40_u8, right: 3_u8)
  exerciseI16(left: -1000_i16, right: 30_i16)
  exerciseU16(left: 1000_u16, right: 30_u16)
  exerciseI32(left: -120000_i32, right: 3000_i32)
  exerciseU32(left: 100000_u32, right: 300_u32)
  exerciseI64(left: -900000_i64, right: 300000_i64)
  exerciseU64(left: 5000000000_u64, right: 1000000000_u64)
  exerciseInt(left: -5000000000_i64, right: 1000000000_i64)
  exerciseUInt(left: 6000000000_u64, right: 3000000000_u64)
}
