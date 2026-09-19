// Expected exit: 0
// Expected stdout:
// i8 -9/-15/-36; compound -22
// u8 43/37/120; compound 82
// i16 -970/-1030/-30000; compound -1944
// u16 1030/970/30000; compound 2056
// i32 -117000/-123000/-360000000; compound -234004
// u32 100300/99700/30000000; compound 200596
// i64 -600000/-1200000/-270000000000; compound -1200004
// u64 6000000000/4000000000/5000000000000000000; compound 11999999996
// Int -4000000000/-6000000000/-5000000000000000000; compound -8000000004
// UInt 9000000000/3000000000/18000000000000000000; compound 17999999996

fn exerciseI8(left: i8, right: i8) {
  let sum = left + right
  let difference = left - right
  let product = left * right
  var compound = left
  compound += right
  compound -= 2_i8
  compound *= 2_i8
  print("i8 ${sum}/${difference}/${product}; compound ${compound}")
}

fn exerciseU8(left: u8, right: u8) {
  let sum = left + right
  let difference = left - right
  let product = left * right
  var compound = left
  compound += right
  compound -= 2_u8
  compound *= 2_u8
  print("u8 ${sum}/${difference}/${product}; compound ${compound}")
}

fn exerciseI16(left: i16, right: i16) {
  let sum = left + right
  let difference = left - right
  let product = left * right
  var compound = left
  compound += right
  compound -= 2_i16
  compound *= 2_i16
  print("i16 ${sum}/${difference}/${product}; compound ${compound}")
}

fn exerciseU16(left: u16, right: u16) {
  let sum = left + right
  let difference = left - right
  let product = left * right
  var compound = left
  compound += right
  compound -= 2_u16
  compound *= 2_u16
  print("u16 ${sum}/${difference}/${product}; compound ${compound}")
}

fn exerciseI32(left: i32, right: i32) {
  let sum = left + right
  let difference = left - right
  let product = left * right
  var compound = left
  compound += right
  compound -= 2_i32
  compound *= 2_i32
  print("i32 ${sum}/${difference}/${product}; compound ${compound}")
}

fn exerciseU32(left: u32, right: u32) {
  let sum = left + right
  let difference = left - right
  let product = left * right
  var compound = left
  compound += right
  compound -= 2_u32
  compound *= 2_u32
  print("u32 ${sum}/${difference}/${product}; compound ${compound}")
}

fn exerciseI64(left: i64, right: i64) {
  let sum = left + right
  let difference = left - right
  let product = left * right
  var compound = left
  compound += right
  compound -= 2_i64
  compound *= 2_i64
  print("i64 ${sum}/${difference}/${product}; compound ${compound}")
}

fn exerciseU64(left: u64, right: u64) {
  let sum = left + right
  let difference = left - right
  let product = left * right
  var compound = left
  compound += right
  compound -= 2_u64
  compound *= 2_u64
  print("u64 ${sum}/${difference}/${product}; compound ${compound}")
}

fn exerciseInt(left: Int, right: Int) {
  let sum = left + right
  let difference = left - right
  let product = left * right
  var compound = left
  compound += right
  compound -= 2_i64
  compound *= 2_i64
  print("Int ${sum}/${difference}/${product}; compound ${compound}")
}

fn exerciseUInt(left: UInt, right: UInt) {
  let sum = left + right
  let difference = left - right
  let product = left * right
  var compound = left
  compound += right
  compound -= 2_u64
  compound *= 2_u64
  print("UInt ${sum}/${difference}/${product}; compound ${compound}")
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
