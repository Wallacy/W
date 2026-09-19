// Expected exit: 0
// Expected stdout:
// Float strict ok
entry {
  let f32Sum = 1.5_f32 + 2.25_f32
  let f32Difference = 9.5_f32 - 5.5_f32
  let f32Product = 1.5_f32 * 2.0_f32
  let f32Quotient = 7.5_f32 / 2.5_f32
  let f32SignedZero = -0.0_f32
  let f32Nan = 0.0_f32 / 0.0_f32
  let f64Sum = 1.5_f64 + 2.25_f64
  let f64Difference = 9.5_f64 - 5.5_f64
  let f64Product = 1.5_f64 * 2.0_f64
  let f64Quotient = 7.5_f64 / 2.5_f64
  let f64SignedZero = -0.0_f64
  let f64Nan = 0.0_f64 / 0.0_f64
  let valid = f32Sum == 3.75_f32 && f32Difference == 4.0_f32 &&
    f32Product == 3.0_f32 && f32Quotient != 4.0_f32 &&
    f32Sum > 3.0_f32 && f32Sum >= 3.75_f32 &&
    f32Quotient < 4.0_f32 && f32Quotient <= 3.0_f32 &&
    f32SignedZero == 0.0_f32 && f32Nan != f32Nan &&
    f64Sum == 3.75_f64 && f64Difference == 4.0_f64 &&
    f64Product == 3.0_f64 && f64Quotient != 4.0_f64 &&
    f64Sum > 3.0_f64 && f64Sum >= 3.75_f64 &&
    f64Quotient < 4.0_f64 && f64Quotient <= 3.0_f64 &&
    f64SignedZero == 0.0_f64 && f64Nan != f64Nan

  if valid {
    print("Float strict ok")
  } else {
    print("Float strict bad")
  }
}
