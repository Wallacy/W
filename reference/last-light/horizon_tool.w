// Module-run design oracle. The source is a normal module with an explicit
// entry descriptor. Its chart dependency is declared by build.w and closed
// by build.w resolution; this file does not carry a virtual root or lock.

module horizon_tool

import chart.science
import { Batch } from std.data

export struct HorizonReading {
  sequence: u64
  flux: f64
}

fn horizonScore(batch: Batch<HorizonReading>): f64 {
  if batch.rowCount() == 0 {
    return 0.0
  }
  return chart.science.score(batch)
}

fn menuFor(score: f64): String {
  if score >= 0.85 {
    return "evacuation"
  }
  if score >= 0.55 {
    return "warning"
  }
  return "steady"
}

fn provenance(score: f64, label: String): String {
  return "horizon:" + label
}

fn runHorizon(): () {
  let batch: Batch<HorizonReading> = chart.science.sample()
  let score = horizonScore(batch)
  let label = menuFor(score)
  let result = provenance(score, label)
  print(result)
}

entry(runHorizon)
