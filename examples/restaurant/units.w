// W Working Draft — pseudocódigo pedagógico, não executável.
// Quantidades são types compile-time; a sintaxe de literal segue em W-O036.

export type Temperature = Quantity<si.Kelvin, f64>
export type TemperatureDelta = Quantity<si.KelvinDelta, f64>
export type TemperatureRate = Quantity<si.KelvinPerSecond, f64>
export type Area = Quantity<si.SquareMeter, f64>
export type Mass = Quantity<si.Kilogram, f64>
export type MassFlow = Quantity<si.KilogramPerSecond, f64>
export type Pressure = Quantity<si.Pascal, f64>
export type Power = Quantity<si.Watt, f64>
export type Energy = Quantity<si.Joule, f64>
export type ThermalCapacity = Quantity<si.JoulePerKelvin, f64>
export type ThermalTransmittance = Quantity<si.WattPerSquareMeterKelvin, f64>
export type FlowResistance = Quantity<si.PascalSecondSquaredPerKilogramSquared, f64>
export type Ratio = f64 where self >= 0.0 && self <= 1.0

export fn clampRatio(value: f64): Ratio {
  if value < 0.0 {
    return 0.0
  }
  if value > 1.0 {
    return 1.0
  }
  return value
}

export fn absolute(value: TemperatureDelta): TemperatureDelta {
  if value < 0.0_KelvinDelta {
    return -value
  }
  return value
}

export fn absoluteRate(value: TemperatureRate): TemperatureRate {
  if value < 0.0_KelvinPerSecond {
    return -value
  }
  return value
}
