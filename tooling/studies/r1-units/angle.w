// R1 Last Light unit-delimiter study variant.

import si from std

struct ImpactEstimate {
  energy: f64
  fallTime: f64
}

fn estimateHorizonImpact(mass: f64, named height: f64): ImpactEstimate {
  let gravity = 9.80665<si.m/si.s^2>
  let energy = mass * gravity.value * height
  let fallTime = (2.0 * height / gravity.value) ** 0.5
  return ImpactEstimate(energy: energy, fallTime: fallTime)
}

test "angle units preserve the horizon estimate" for estimateHorizonImpact {
  let result = estimateHorizonImpact(2.0, height: 5.0)
  expect result.energy > 98.0
  expect result.energy < 99.0
}
