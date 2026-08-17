// SI source projection used by quantity literals.
//
// Unit names are exported bindings. A caller must import a name or use the
// module binding (`si.ms`); there is no ambient unit registry.
// Base declarations appear before derived declarations for readability. The
// one `unit name: Dimension` declaration is the reference for that dimension;
// reference resolution is declaration-order independent, while derived scales
// still resolve only against declarations in this module.

export dimension Length
export dimension Mass
export dimension Duration
export dimension Temperature
export dimension TemperatureDelta
export dimension ElectricCurrent
export dimension Amount
export dimension LuminousIntensity
export dimension Angle
export dimension Energy
export dimension Power
export dimension Frequency
export dimension Velocity
export dimension Acceleration
export dimension Pressure
export unit m: Length
export unit kg: Mass
export unit s: Duration
export unit K: Temperature
export unit deltaK: TemperatureDelta
export unit A: ElectricCurrent
export unit mol: Amount
export unit cd: LuminousIntensity
export unit rad: Angle
export unit ms = 1/1_000<s>
export unit us = 1/1_000<ms>
export unit ns = 1/1_000<us>
export unit min = 60<s>
export unit h = 60<min>
export unit Hz = 1/s
export unit N = 1<kg*m/s^2>
export unit J = 1<N*m>
export unit W = 1<J/s>
export unit Pa = 1<N/m^2>
export unit ps = 1/1_000<ns>
