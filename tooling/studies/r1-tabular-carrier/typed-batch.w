// R1 Last Light tabular-carrier study variant.
// Selected direction: finite owned columnar data.Batch with a closed schema.

import data from std

struct HorizonReading: data.Row {
  let sequence: u64
  let hawkingFlux: f64
  let warning: String?
}

fn summarizeHorizon(_ batch: data.Batch<HorizonReading>): f64? {
  let column = batch.column(.hawkingFlux)
  var maximum: f64? = .none
  for index in 0..<batch.rows {
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

test "typed batch uses a generated field descriptor" for summarizeHorizon {
  let readings: data.Batch<HorizonReading> = batch
  let flux = summarizeHorizon(readings)
  expect flux == .some(0.8)
}
