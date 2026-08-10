// Satellite swarm around the Restaurant at the End of the Universe.

import si from std
import { Tensor } from std.tensor
import { Distance, PhysicalDuration, Velocity } from units

export type SatelliteId = u32
export alias Vector3<T> = Tensor<T, shape: [3]>

export struct StateVector {
  position: Vector3<Distance>
  velocity: Vector3<Velocity>
  epoch: PhysicalDuration
}

export enum SatelliteHealth {
  nominal
  degraded(reason: String)
  silent(since: PhysicalDuration)
  lost
}

export struct SatelliteTelemetry {
  id: SatelliteId
  state: StateVector
  health: SatelliteHealth
  sequence: u64
}

export enum SatelliteError: Error {
  unavailable(SatelliteId)
  stale(expectedAfter: u64, found: u64)
  navigation
  service(ServiceFailure)
}

export protocol SatelliteApi {
  async fn telemetry(after sequence: u64): SatelliteTelemetry throws SatelliteError
  async fn follow(
    after sequence: u64,
  ): some Stream<SatelliteTelemetry, SatelliteError>
  async fn command(next: take StateVector, sequence: u64): () throws SatelliteError
}

export fn propagate(state: ref StateVector, during elapsed: PhysicalDuration): StateVector {
  return StateVector(
    position: state.position + state.velocity * elapsed,
    velocity: state.velocity,
    epoch: state.epoch + elapsed,
  )
}

export struct PairTelemetry {
  left: SatelliteTelemetry
  right: SatelliteTelemetry
}

export async fn observePair(
  left: ServiceRef<SatelliteApi>,
  right: ServiceRef<SatelliteApi>,
  after sequence: u64,
): PairTelemetry throws SatelliteError {
  async let leftSample = left.telemetry(after: sequence)
  async let rightSample = right.telemetry(after: sequence)
  let (leftSample, rightSample) = try await (leftSample, rightSample)
  return PairTelemetry(left: leftSample, right: rightSample)
}

export async fn collectTelemetry(
  satellite: ServiceRef<SatelliteApi>,
  after sequence: u64,
  maximum: usize<(1...256)>,
): Array<SatelliteTelemetry> throws SatelliteError {
  let remote = try await satellite.follow(after: sequence)
  var feed = (take remote).buffer(capacity: 8)
  var samples = Array<SatelliteTelemetry>(minimumCapacity: maximum)

  for try await sample in feed {
    samples.append(take sample)

    if samples.count == maximum {
      break
    }
  }

  return samples
}

export struct CollisionWindow {
  left: SatelliteId
  right: SatelliteId
  separation: Distance
  at: PhysicalDuration
}

export fn closestApproach(
  leftId: SatelliteId,
  rightId: SatelliteId,
  left: ref StateVector,
  right: ref StateVector,
  samples: usize<(1...4_096)>,
  step: PhysicalDuration,
): CollisionWindow {
  var bestIndex = 0_usize
  var bestDistance = Distance.MAX

  for index in 0..<samples {
    let elapsed = index * step
    let leftState = propagate(left, during: elapsed)
    let rightState = propagate(right, during: elapsed)
    let distance = (leftState.position - rightState.position).norm()

    if distance < bestDistance {
      bestDistance = distance
      bestIndex = index
    }
  }

  return CollisionWindow(
    left: leftId,
    right: rightId,
    separation: bestDistance,
    at: bestIndex * step,
  )
}

test "constant velocity propagation preserves shape" for propagate {
  let state = StateVector(
    position: [0<si.m>, 0<si.m>, 0<si.m>],
    velocity: [1<si.m/si.s>, 2<si.m/si.s>, 3<si.m/si.s>],
    epoch: 0<si.s>,
  )
  let next = propagate(state, during: 2<si.s>)
  expect next.position == [2<si.m>, 4<si.m>, 6<si.m>]
}
