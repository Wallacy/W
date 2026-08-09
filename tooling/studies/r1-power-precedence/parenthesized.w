// R1 expression-core study variant.

import si from std

fn powerSamples(): (f64, f64, Int, Int) {
  let negativeBase = -(2.0 ** 2)
  let negativeExponent = 2.0 ** (-3)
  let associated = 2 ** (3 ** 2)
  let bitwise = 5 ^ 3
  return (negativeBase, negativeExponent, associated, bitwise)
}

fn explicitParentheses(): (f64, Int) {
  let parenthesizedBase = (-2.0) ** 2
  let leftAssociated = (2 ** 3) ** 2
  return (parenthesizedBase, leftAssociated)
}

fn unitSample(): f64 {
  let gravity: Quantity<si.Acceleration, f64> = 9.81<m/s^2>
  return gravity.canonicalValue
}

test "parentheses preserve the primary power outcome" {
  let (negativeBase, negativeExponent, associated, bitwise) = powerSamples()
  expect negativeBase == -4.0
  expect negativeExponent == 0.125
  expect associated == 512
  expect bitwise == 6
  expect unitSample() == 9.81
}
