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
  enabled: Bool
  name: String
  timeout: PhysicalDuration
  tables: usize
  courses: usize
  buffer: usize
}

fn start<
  enabled: Bool,
  name: String,
  timeout: PhysicalDuration,
  tables: usize,
  courses: usize,
  buffer: usize,
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

alias KitchenProfile = ServiceProfile<
  enabled: true,
  name: "kitchen",
  timeout: 250<si.ms>,
  tables: 8,
  courses: 4,
  buffer: 4096,
>

fn observeStaticContract<
  enabled: Bool,
  name: String,
  timeout: PhysicalDuration,
  tables: usize,
  courses: usize,
  buffer: usize,
>(): StartedService {
  return start<
    enabled: enabled,
    name: name,
    timeout: timeout,
    buffer: buffer,
    courses: courses,
    tables: tables,
  >()
}

test "named static slots preserve the service profile" for observeStaticContract {
  let service = observeStaticContract<
    enabled: true,
    name: "kitchen",
    timeout: 250<si.ms>,
    courses: 4,
    tables: 8,
    buffer: 4096,
  >()
  let horizon = observeStaticContract<
    enabled: false,
    name: "horizon",
    timeout: 1750<si.ms>,
    tables: 2,
    courses: 9,
    buffer: 64,
  >()
  expect KitchenProfile.active
  expect KitchenProfile.tables == 8
  expect service.name == "kitchen"
  expect service.tables == 8
  expect service.courses == 4
  expect !horizon.enabled
  expect horizon.tables == 2
  expect horizon.courses == 9
}
