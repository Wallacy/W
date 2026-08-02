// Physical and fictional units used by the Last Light.

import si from std
import iec from std

export dimension Applause
export unit clap: Applause
export unit ovation = 1_000<clap>

export unit smoot = 1.7018<si.m>
export unit kiloSmoot = 1_000<smoot>

export unit degC = Unit.affine(reference: si.K, scale: 1, offset: 27315 / 100)
export unit degF = Unit.affine(reference: si.K, scale: 5 / 9, offset: 45967 / 180)

export alias Temperature = Quantity<si.Temperature, f64>
export alias TemperatureDelta = Quantity<si.TemperatureDelta, f64>
export alias Power = Quantity<si.Power, f64>
export alias Energy = Quantity<si.Energy, f64>
export alias PhysicalDuration = Quantity<si.Duration, f64>
export alias Distance = Quantity<si.Length, f64>
export alias Velocity = Quantity<si.Velocity, f64>
export alias Acceleration = Quantity<si.Acceleration, f64>
export alias Mass = Quantity<si.Mass, f64>
export alias Frequency = Quantity<si.Frequency, f64>
export alias Pressure = Quantity<si.Pressure, f64>
export alias MemorySize = Quantity<iec.Information, u64>
export alias ApplauseLevel = Quantity<Applause, u64>

export const serviceTemperature = 180<degC>
export const calibrationDistance = 2<smoot>
export const applauseThreshold = 3<ovation>
export const commandLimit = 64<KiB>

export fn energy(power: Power, during duration: PhysicalDuration): Energy {
  return power * duration
}

export fn temperatureDifference(from lower: Temperature, to upper: Temperature): TemperatureDelta {
  return upper - lower
}
