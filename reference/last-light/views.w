// Borrowed projections for the Last Light restaurant.

import * from std.ffi

foreign c from "last_light_telemetry.h" {
  fn ll_telemetry_write(
    data: c.ptr<c.uchar>,
    size: c.size,
  ): c.int
}

export enum TelemetryError: Error {
  writeFailed(status: c.int)
}

export fn serviceTemperatures(
  values: ref Array<f64>,
): view Array<f64> {
  guard values.count >= 2 else return values[0..<0]
  return values[1..<values.count]
}

export fn correctTemperatures(
  values: inout Array<f64>,
  offset correction: f64,
): inout view Array<f64> {
  let inout corrected: view Array<f64> = values[1..<values.count]

  for inout temperature in corrected {
    temperature += correction
  }

  return corrected
}

export fn commandVerb(line: ref String): view String? {
  return line.scalars
    .split(where: (scalar) => scalar.isWhitespace)
    .first
}

export fn writeTelemetry(
  payload: view Bytes,
): usize throws TelemetryError {
  return payload.withPointer(
    (pointer, count) => unsafe {
      let status = ll_telemetry_write(pointer, count)
      guard status >= 0 else throw .writeFailed(status: status)
      return usize(status)
    },
  )
}

test "a read-only view does not expose owner capacity" for serviceTemperatures {
  var readings = [2.70, 42.0, 273.15, 0.0]
  let service = serviceTemperatures(readings)

  expect service == [42.0, 273.15, 0.0]
  expect service.count == 3

  // Compile-fail assay: a view has no allocator or capacity.
  // print(service.capacity)

  // Compile-fail assay: append can move storage borrowed by service.
  // readings.append(1.0)
  print(service[0])
}

test "an exclusive view mutates elements but not its extent" for correctTemperatures {
  var readings = [2.70, 41.5, 272.65]
  let inout corrected = correctTemperatures(inout readings, offset: 0.5)

  expect corrected == [42.0, 273.15]
  expect readings == [2.70, 42.0, 273.15]

  // Compile-fail assay: an inout view cannot resize its owner.
  // corrected.append(0.0)
}

test "a string view validates text boundaries and materializes explicitly" for commandVerb {
  let command = "serve horizon-cake"
  guard let verb = commandVerb(command) else panic("verb is missing")

  expect verb == "serve"
  let owned = verb.materialize()
  expect owned == "serve"

  // Compile-fail assay: String mutation waits for the borrowed view to end.
  // command.append(" now")
  print(verb)
}
