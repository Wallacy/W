/// Type identity and opt-in metadata without a universal dynamic value.
import { Course, OrderId } from domain

export struct ReservationKey: Hashable & Reflectable {
  orderId: OrderId
  course: Course
}

export alias T = ReservationKey

export object OvenIdentity: Reflectable {
  export serial: String
  secretCalibration: i32

  export init(serial: String, secretCalibration: i32) {
    self.serial = serial
    self.secretCalibration = secretCalibration
  }
}

export enum KitchenSignal: Reflectable {
  orderAccepted(OrderId)
  heatWarning(i32)
  universeEnded
}

export alias ActionableSignal =
  KitchenSignal<[.orderAccepted, .heatWarning]>

fn contextualQueryNames(reflect: Int, info: Int, of: Int, typeof: Int): Int {
  return reflect
}

export fn reservationKeyInfo(): ref TypeInfo {
  return info of ReservationKey
}

export fn reservationKeyType(value: ref any Hashable): TypeId {
  return type of value
}

export fn reservationKeyOrder(value: ref any Hashable): OrderId? {
  let exact = value is ReservationKey
  if let ref key = value as? ReservationKey {
    return key.orderId
  }
  return .none
}

fn identityInvariants(value: ref any Hashable) {
  let exact = value is ReservationKey
  let sameType = type of value == type of ReservationKey
  let ref payload = value as? ReservationKey

  expect exact == sameType
  expect (payload != .none) == exact
}

fn metadataInvariants(value: ref any Reflectable) {
  let ref staticInfo = info of ReservationKey
  let ref dynamicInfo = info of value

  expect staticInfo.id == type of ReservationKey
  expect dynamicInfo.id == type of value
}

export fn reflectedName(
  value: ref any Reflectable,
): view String {
  let T = value
  let staticHomonym = type of T
  let dynamicHomonym = type of (T)
  let ref metadata = info of value
  return metadata.name
}

test "type identity is local and exact" for reservationKeyInfo {
  let first = type of ReservationKey
  let second = type of ReservationKey

  expect first == second
  expect first != type of KitchenSignal
}

test "reflection preserves visibility and enum subsets" for reflectedName {
  let ref ovenInfo = info of OvenIdentity
  let ref signalInfo = info of ActionableSignal

  expect ovenInfo.properties.count == 1
  expect ovenInfo.properties[0].name == "serial"
  expect signalInfo.cases.count == 2
}
