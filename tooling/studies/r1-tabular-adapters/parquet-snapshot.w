// R1 TAB1 variant B: Parquet snapshot archive.
// The positional source is explicit. The study does not claim reader execution.

import data from std.data
import io from std.io
import parquet from std.parquet

struct HorizonReading: data.Row {
  sequence: u64
  hawkingFlux: f64
  warning: String?
}

async fn summarizeParquet<Failure: Error, Source: io.SnapshotByteSource<Failure>>(
  source: take Source,
  options: parquet.DecodeOptions,
): f64? throws parquet.DecodeError<Failure> {
  var batches = parquet.decode<HorizonReading, Failure, Source>(
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
