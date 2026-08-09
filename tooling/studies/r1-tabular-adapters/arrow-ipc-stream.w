// R1 TAB1 variant C: Arrow IPC stream service handoff.
// The stream source and typed rows stay explicit. This is a design oracle.

import data from std.data
import arrow from std.arrow
import io from std.io

struct HorizonReading: data.Row {
  sequence: u64
  hawkingFlux: f64
  warning: String?
}

async fn summarizeArrow<Failure: Error, Source: io.ByteSource<Failure>>(
  source: take Source,
  options: arrow.DecodeOptions,
): f64? throws arrow.DecodeError<Failure> {
  var batches = arrow.decodeIpcStream<HorizonReading, Failure, Source>(
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
