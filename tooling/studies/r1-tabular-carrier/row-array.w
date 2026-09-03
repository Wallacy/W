// R1 Last Light tabular-carrier study variant.
// Baseline: Array<Row> remains useful for row-centric algorithms.

struct HorizonReading {
  let sequence: u64
  let hawkingFlux: f64
  let warning: String?
}

fn summarizeHorizon(_ readings: ref Array<HorizonReading>): f64? {
  var maximum: f64? = .none
  for reading in readings {
    if !reading.hawkingFlux.isFinite { continue }
    if let current = maximum {
      if reading.hawkingFlux > current { maximum = .some(reading.hawkingFlux) }
    } else {
      maximum = .some(reading.hawkingFlux)
    }
  }
  return maximum
}

fn convenientRowAlgorithm(_ readings: ref Array<HorizonReading>): u64 {
  return readings.lazy.map((reading) => reading.sequence).max() ?? 0
}

test "row arrays stay convenient for row-centric algorithms" for summarizeHorizon {
  let readings = [
    HorizonReading(sequence: 7, hawkingFlux: 0.2, warning: .none),
    HorizonReading(sequence: 8, hawkingFlux: 0.8, warning: "evacuate"),
  ]
  expect summarizeHorizon(readings) == .some(0.8)
}
