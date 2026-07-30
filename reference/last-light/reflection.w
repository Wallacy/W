/// Opt-in runtime reflection without a universal dynamic value.
import std.reflect as reflect
import { Course, OrderId } from restaurant.domain

export struct ReservationKey: Hashable & reflect.Reflectable {
  orderId: OrderId
  course: Course
}

export object OvenIdentity: reflect.Reflectable {
  export serial: String
  secretCalibration: i32

  export init(serial: String, secretCalibration: i32) {
    self.serial = serial
    self.secretCalibration = secretCalibration
  }
}

export enum KitchenSignal: reflect.Reflectable {
  orderAccepted(OrderId)
  heatWarning(i32)
  universeEnded
}

export alias ActionableSignal =
  KitchenSignal<[.orderAccepted, .heatWarning]>

export fn reservationKeyInfo(): ref reflect.TypeInfo {
  return reflect.info<ReservationKey>()
}

export fn reflectedName(
  value: ref any reflect.Reflectable,
): view String {
  let ref info = reflect.info(of: value)
  return info.name
}

test "type identity is local and exact" for reservationKeyInfo {
  let first = reflect.TypeId.of<ReservationKey>()
  let second = reflect.TypeId.of<ReservationKey>()

  expect first == second
  expect first != reflect.TypeId.of<KitchenSignal>()
}

test "reflection preserves visibility and enum subsets" for reflectedName {
  let ref ovenInfo = reflect.info<OvenIdentity>()
  let ref signalInfo = reflect.info<ActionableSignal>()

  expect ovenInfo.properties.count == 1
  expect ovenInfo.properties[0].name == "serial"
  expect signalInfo.cases.count == 2
}
