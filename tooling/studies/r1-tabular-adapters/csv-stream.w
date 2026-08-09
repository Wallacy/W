// R1 TAB1 variant A: typed CSV stream.
// The study compares workflow clarity. It does not claim codec execution.

import data from std.data
import csv from std.csv
import io from std.io

struct HorizonReading: data.Row {
  sequence: u64
  hawkingFlux: f64
  warning: String?
}

async fn summarizeCsv<Failure: Error, Source: io.ByteSource<Failure>>(
  source: take Source,
  options: csv.DecodeOptions,
): f64? throws csv.DecodeError<Failure> {
  var batches = csv.decode<HorizonReading, Failure, Source>(
    source: take source,
    options: options,
  )
  var maximum: f64? = .none
  while true {
    let next = try await batches.next()
    switch next {
      case .none: return maximum
      case .some(let batch):
        let flux = batch.column(.hawkingFlux)
        var index: usize = 0
        while index < flux.count() {
          let value: f64 = flux.copy(at: index)
          if value.isFinite {
            maximum = switch maximum {
              case .none: .some(value)
              case .some(let current): .some(if value > current { value } else { current })
            }
          }
          index += 1
        }
    }
  }
}
