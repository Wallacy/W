import { Temperature } from restaurant.units

export fn temperatureValue(temperature: Temperature): f64 {
  return temperature.value
}
