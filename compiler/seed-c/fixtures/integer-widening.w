// Expected exit: 0
// Expected stdout:
// Widen -7/200/202/203

fn widenReturn(value: i8): i16 {
  return value
}

fn accept(value: i16): i16 {
  return value
}

entry {
  let returned = widenReturn(value: -7_i8)
  let called = accept(value: 200_u8)
  let unsigned: u16 = 202_u8
  let alias: Int = 203_u8
  print("Widen ${returned}/${called}/${unsigned}/${alias}")
}
