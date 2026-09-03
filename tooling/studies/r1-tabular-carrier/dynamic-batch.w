// R1 Last Light tabular-carrier study variant.
// Complementary direction: runtime schema binds to an explicit static row type.

import data from std

struct HorizonReading: data.Row {
  let sequence: u64
  let hawkingFlux: f64
  let warning: String?
}

fn summarizeHorizon(_ batch: data.DynamicBatch): f64? throws data.BindError {
  let typed = try (take batch).bind<HorizonReading>(copyPolicy: .never)
  let column = typed.column(.hawkingFlux)
  var maximum: f64? = .none
  for index in 0..<typed.rows {
    let reading: f64 = column[index]
    if !reading.isFinite { continue }
    if let current = maximum {
      if reading > current { maximum = .some(reading) }
    } else {
      maximum = .some(reading)
    }
  }
  return maximum
}

test "dynamic binding checks names and types before publication" for summarizeHorizon {
  let readings: data.DynamicBatch = runtimeBatch
  let flux = try summarizeHorizon(readings)
  expect flux == .some(0.8)
}
