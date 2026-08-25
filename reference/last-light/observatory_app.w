// Native observatory process for the satellite swarm and event horizon.

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process
import {
  HorizonError,
  HorizonMonitorApi,
  HorizonStatus,
} from horizon
import service { HorizonMonitorApi as horizonMonitor } from horizon
import {
  SatelliteApi,
  SatelliteError,
  SatelliteId,
  observePair,
} from orbit
import service { SatelliteApi as satelliteSwarm<key: SatelliteId> } from orbit

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
  args: ProcessArguments,
  ctx: ProcessContext,
): ProcessExitCode throws ObservatoryError {
  let left = try await satelliteSwarm.at(1)
  let right = try await satelliteSwarm.at(2)
  let horizon = horizonMonitor

  let pairTask = async observePair(left, right: right, after: 0)
  let horizonTask = async horizon.status(after: 0)
  let (telemetry, horizonStatus) = try await (pairTask, horizonTask)
  let horizonLabel = horizonStatusLabel(horizonStatus)

  print(
    "Satellite ${telemetry.left.id}: ${telemetry.left.health}; "
      + "satellite ${telemetry.right.id}: ${telemetry.right.health}; "
      + "event horizon: ${horizonLabel}.",
  )
  return .success
}

entry LastLightObservatory(runObservatory)
