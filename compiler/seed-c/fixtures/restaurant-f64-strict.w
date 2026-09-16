entry {
  let sum = 1.5 + 2.25
  let difference = 9.5 - 5.5
  let product = 1.5 * 2.0
  let quotient = 7.5 / 2.5
  let signedZero = -0.0
  let nan = 0.0 / 0.0
  let valid = sum == 3.75 && difference == 4.0 && product == 3.0 && quotient != 4.0 && sum > 3.0 && sum >= 3.75 && quotient < 4.0 && quotient <= 3.0 && signedZero == 0.0 && nan != nan
  if valid {
    print("Float strict ok")
  } else {
    print("Float strict bad")
  }
}
