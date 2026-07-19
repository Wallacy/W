// W Working Draft — baseline parseável para uma boundary C; não executável.
// Raw ABI fica local. O wrapper restaura errors, units e estado observável.

import { Temperature } from restaurant.units

foreign c from "restaurant_equipment.h" {
  type restaurant_equipment
  fn restaurant_read_probe(
    _ handle: c.ptr<restaurant_equipment>,
    _ probe: c.int,
    _ outCelsius: c.ptr<c.double>,
  ): c.int
}

export enum Probe {
  cavity
  food
  ambient
}

export enum EquipmentInteropError: Error {
  readFailed(c.int)
  nonFiniteTemperature
}

export protocol EquipmentApi {
  mut fn read(probe: Probe): Temperature throws EquipmentInteropError
}

// A implementação e o raw pointer não fazem parte da interface do módulo.
object EquipmentDevice {
  handle: c.ptr<restaurant_equipment>

  mut fn read(probe: Probe): Temperature throws EquipmentInteropError {
    var value = 0.0_f64
    let status = restaurant_read_probe(handle, probe.rawValue, c.address(inout value))
    guard status == 0 else {
      throw .readFailed(status)
    }
    guard value.isFinite else {
      throw .nonFiniteTemperature
    }
    return Temperature.from(value, unit: si.Celsius)
  }
}
