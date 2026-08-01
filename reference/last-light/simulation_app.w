// Standalone deterministic target that does not require a service deployment.

import std.process as process
import {
  SimulationError,
  simulateShift,
  writeSimulation,
} from restaurant.simulation
import { SimulationProfile } from restaurant.domain

async fn runSimulation(
  args: process.Arguments,
  ctx: process.Context,
): process.ExitCode throws SimulationError {
  for profile in [
    SimulationProfile.quietOrbit,
    SimulationProfile.photonRush,
    SimulationProfile.timelineCollision,
  ] {
    let report = try simulateShift(profile)
    var output = String()
    writeSimulation(report, to: output)
    print(output)
  }

  return .success
}

entry LastLightSimulation(runSimulation)
