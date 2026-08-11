// R1 Last Light static-contract-slot study variant.

import si from std
import { PhysicalDuration } from units

struct ServiceProfile<
  _ enabled: Bool,
  _ name: String,
  _ timeout: PhysicalDuration,
  _ tables: usize,
  _ courses: usize,
  _ buffer: usize,
> {
  export const active = enabled
}

struct StartedService {
  enabled: Bool
  name: String
  timeout: PhysicalDuration
  tables: usize
  courses: usize
  buffer: usize
}

fn start<
  _ enabled: Bool,
  _ name: String,
  _ timeout: PhysicalDuration,
  _ tables: usize,
  _ courses: usize,
  _ buffer: usize,
>(): StartedService {
  return StartedService(
    enabled: enabled,
    name: name,
    timeout: timeout,
    tables: tables,
    courses: courses,
    buffer: buffer,
  )
}

alias KitchenProfile = ServiceProfile<true, "kitchen", 250<si.ms>, 8, 4, 4096>

fn observeStaticContract<
  _ enabled: Bool,
  _ name: String,
  _ timeout: PhysicalDuration,
  _ tables: usize,
  _ courses: usize,
  _ buffer: usize,
>(): StartedService {
  return start<enabled, name, timeout, tables, courses, buffer>()
}

test "positional static slots preserve the service profile" for observeStaticContract {
  let service = observeStaticContract<true, "kitchen", 250<si.ms>, 8, 4, 4096>()
  let horizon = observeStaticContract<false, "horizon", 1750<si.ms>, 2, 9, 64>()
  expect KitchenProfile.active
  expect KitchenProfile.tables == 8
  expect service.name == "kitchen"
  expect service.tables == 8
  expect service.courses == 4
  expect !horizon.enabled
  expect horizon.tables == 2
  expect horizon.courses == 9
}
