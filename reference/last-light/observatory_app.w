// Native observatory process for the satellite swarm and event horizon.

import std.process as process
import {
  HorizonError,
  HorizonStatus,
  horizonMonitor,
} from restaurant.horizon
import {
  SatelliteError,
  observePair,
  satelliteSwarm,
} from restaurant.orbit

enum ObservatoryError: Error {
  horizon(HorizonError)
  satellite(SatelliteError)
  service(ServiceFailure)
}

const fn horizonStatusLabel(status: ref HorizonStatus): String {
  return switch status {
    case .stable: "stable"
    case .warning(_): "warning"
    case .evacuation(_): "evacuation"
  }
}

async fn runObservatory(
  args: process.Arguments,
  ctx: process.Context,
): process.ExitCode throws ObservatoryError {
  let left = try await ctx.services.get(satelliteSwarm, key: 1)
  let right = try await ctx.services.get(satelliteSwarm, key: 2)
  let horizon = try await ctx.services.get(horizonMonitor)

  async<.network> let pair = observePair(left, right: right, after: 0)
  async<.network> let horizonStatus = horizon.status(after: 0)
  let (telemetry, horizonStatus) = try await (pair, horizonStatus)
  let horizonLabel = horizonStatusLabel(horizonStatus)

  print(
    "Satellite ${telemetry.left.id}: ${telemetry.left.health}; "
      + "satellite ${telemetry.right.id}: ${telemetry.right.health}; "
      + "event horizon: ${horizonLabel}.",
  )
  return .success
}

entry LastLightObservatory(runObservatory)
