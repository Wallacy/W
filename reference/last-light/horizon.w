// Sensor fusion for the black hole at the final service window.

import { Tensor } from std.tensor
import {
  PhysicalDuration,
  Frequency,
  Pressure,
  Temperature,
} from units

export type SensorId = u32
export type EventSequence = u64

export struct HorizonSample {
  sensor: SensorId
  eventTime: PhysicalDuration
  observedTime: PhysicalDuration
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
  invalidScore(found: f32)
  outOfOrder(previous: EventSequence, found: EventSequence)
  service(ServiceFailure)
}

export protocol HorizonMonitorApi {
  async fn status(after sequence: EventSequence): HorizonStatus throws HorizonError
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
  let status = try classifyHorizon(maximum)

  return HorizonForecast(
    status: status,
    normalized: take centered,
    anomalyBySample: take energy,
  )
}

export fn classifyHorizon(
  score: f32,
): HorizonStatus throws HorizonError<[.nonFinite, .invalidScore]> {
  guard score.isFinite else throw .nonFinite
  guard score >= 0.0 else throw .invalidScore(found: score)

  return switch score {
    case 0.85...: .evacuation(score: score)
    case 0.55..<0.85: .warning(score: score)
    case 0.0..<0.55: .stable
  }
}

test "anomaly thresholds are exhaustive" for classifyHorizon {
  let safe = try classifyHorizon(0.2)
  let alert = try classifyHorizon(0.7)
  let evacuation = try classifyHorizon(0.9)

  expect safe in (.stable)
  expect alert in (.warning)
  expect evacuation in (.evacuation)
}

test "a negative score keeps its typed error" for classifyHorizon {
  do {
    let _ = try classifyHorizon(-0.1)
    panic("negative horizon score was accepted")
  } catch .invalidScore(let found) {
    expect found == -0.1
  } catch .nonFinite {
    panic("negative finite score became non-finite")
  }
}
