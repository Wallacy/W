// Sensor fusion for the black hole at the final service window.

import std.tensor
import {
  Duration,
  Frequency,
  Pressure,
  Temperature,
} from restaurant.units

export type SensorId = u32
export type EventSequence = u64

export struct HorizonSample {
  sensor: SensorId
  eventTime: Duration
  observedTime: Duration
  frequency: Frequency
  pressure: Pressure
  temperature: Temperature
  sequence: EventSequence
}

export enum HorizonStatus {
  stable
  warning(score: f32)
  evacuation(score: f32)
}

export struct HorizonWindow<const samples: usize> {
  features: Tensor<f32, shape: [samples, 6]>
  firstSequence: EventSequence
  lastSequence: EventSequence
}

export struct HorizonForecast<const samples: usize> {
  status: HorizonStatus
  normalized: Tensor<f32, shape: [samples, 6]>
  anomalyBySample: Tensor<f32, shape: [samples]>
}

export enum HorizonError: Error {
  emptyWindow
  nonFinite
  outOfOrder(previous: EventSequence, found: EventSequence)
}

fn validate<const samples: usize>(
  window: ref HorizonWindow<samples>,
): () throws HorizonError {
  guard samples > 0 else throw .emptyWindow
  guard window.features.all((value) => value.isFinite) else throw .nonFinite
  guard window.lastSequence >= window.firstSequence else {
    throw .outOfOrder(previous: window.firstSequence, found: window.lastSequence)
  }
}

export fn forecast<const samples: usize>(
  window: ref HorizonWindow<samples>,
  calibration: ref Tensor<f32, shape: [6, 6]>,
): HorizonForecast<samples> throws HorizonError {
  try validate(window)

  let calibrated = window.features @ calibration
  let means = calibrated.mean(axis: 0, mode: .reproducible)
  let centered = calibrated - means.broadcast(to: [samples, 6])
  let energy = (centered * centered).sum(axis: 1, mode: .reproducible)
  let maximum = energy.max()

  let status: HorizonStatus = switch maximum {
    case 0.85...: .evacuation(score: maximum)
    case 0.55..<0.85: .warning(score: maximum)
    case ..<0.55: .stable
  }

  return HorizonForecast(
    status: status,
    normalized: take centered,
    anomalyBySample: take energy,
  )
}

test "anomaly thresholds are exhaustive" {
  let safe: HorizonStatus = .stable
  let alert: HorizonStatus = .warning(score: 0.7)
  expect safe in (.stable)
  expect alert in (.warning)
}
