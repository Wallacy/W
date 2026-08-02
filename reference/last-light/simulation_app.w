// Standalone deterministic target that does not require a service deployment.

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process
import {
  SimulationError,
  simulateShift,
  writeSimulation,
} from simulation
import { SimulationProfile } from domain

async fn runSimulation(
  args: ProcessArguments,
  ctx: ProcessContext,
): ProcessExitCode throws SimulationError {
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
