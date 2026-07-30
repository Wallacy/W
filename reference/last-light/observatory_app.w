// Native observatory process for the satellite swarm.

import {
  SatelliteError,
  observePair,
  satelliteSwarm,
} from restaurant.orbit

async fn runObservatory(
  args: ProcessArguments,
  ctx: ProcessContext,
): ExitCode throws SatelliteError {
  let left = try await ctx.services.get(satelliteSwarm, key: 1)
  let right = try await ctx.services.get(satelliteSwarm, key: 2)
  let telemetry = try await observePair(left, right: right, after: 0)

  print(
    "Satellite ${telemetry.left.id}: ${telemetry.left.health}; "
      + "satellite ${telemetry.right.id}: ${telemetry.right.health}.",
  )
  return .success
}

entry LastLightObservatory(runObservatory)
