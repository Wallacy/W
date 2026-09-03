// R1 Last Light static-contract-slot study variant.

import si from std
import { PhysicalDuration } from units

struct ServiceProfile<
  enabled: Bool,
  name: String,
  timeout: PhysicalDuration,
  tables: usize,
  courses: usize,
  buffer: usize,
> {
  export const active = enabled
}

struct StartedService {
  let enabled: Bool
  let name: String
  let timeout: PhysicalDuration
  let tables: usize
  let courses: usize
  let buffer: usize
}

fn start<
  _ enabled: Bool,
  _ tables: usize,
  _ courses: usize,
  _ buffer: usize,
>(name: String, timeout: PhysicalDuration): StartedService {
  return StartedService(
    enabled: enabled,
    name: name,
    timeout: timeout,
    tables: tables,
    courses: courses,
    buffer: buffer,
  )
}

alias KitchenProfile = ServiceProfile<
  enabled: true,
  name: "kitchen",
  timeout: 250<si.ms>,
  tables: 8,
  courses: 4,
  buffer: 4096,
>

fn observeStaticContract<
  _ enabled: Bool,
  _ tables: usize,
  _ courses: usize,
  _ buffer: usize,
>(name: String, timeout: PhysicalDuration): StartedService {
  return start<enabled, tables, courses, buffer>(name: name, timeout: timeout)
}

test "a split call preserves the service profile" for observeStaticContract {
  let service = observeStaticContract<true, 8, 4, 4096>(
    name: "kitchen",
    timeout: 250<si.ms>,
  )
  let horizon = observeStaticContract<false, 2, 9, 64>(
    name: "horizon",
    timeout: 1750<si.ms>,
  )
  expect KitchenProfile.active
  expect KitchenProfile.tables == 8
  expect service.name == "kitchen"
  expect service.tables == 8
  expect service.courses == 4
  expect !horizon.enabled
  expect horizon.tables == 2
  expect horizon.courses == 9
}
