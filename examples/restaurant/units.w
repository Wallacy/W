// Physical and fictional units used by the Last Light.

import std.si
import std.iec

export dimension Applause
export unit clap: Applause
export unit ovation = 1_000<clap>

export unit smoot = 1.7018<si.m>
export unit kiloSmoot = 1_000<smoot>

export unit degC = Unit.affine(
  reference: si.K,
  scale: 1,
  offset: 27315 / 100,
)

export unit degF = Unit.affine(
  reference: si.K,
  scale: 5 / 9,
  offset: 45967 / 180,
)

export type Temperature = Quantity<si.Temperature, f64>
export type TemperatureDelta = Quantity<si.TemperatureDelta, f64>
export type Power = Quantity<si.Power, f64>
export type Energy = Quantity<si.Energy, f64>
export type Duration = Quantity<si.Duration, f64>
export type Distance = Quantity<si.Length, f64>
export type MemorySize = Quantity<iec.Information, u64>

export const serviceTemperature = 180<degC>
export const calibrationDistance = 2<smoot>
export const applauseThreshold = 3<ovation>
export const commandLimit = 64<KiB>

export fn energy(power: Power, during duration: Duration): Energy {
  return power * duration
}

export fn temperatureDifference(
  from lower: Temperature,
  to upper: Temperature,
): TemperatureDelta {
  return upper - lower
}
