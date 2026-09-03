// Borrowed projections for the Last Light restaurant.

import * from std.ffi

foreign c from "last_light_telemetry.h" {
  fn ll_telemetry_write(
    _ data: c.ptr<c.uchar>,
    _ size: c.size,
  ): c.int
}

export enum TelemetryError: Error {
  writeFailed(status: c.int)
}

export struct MenuCourse {
  let title: String
  let allergens: Array<String>
  let supplierContract: String
}

// A nominal borrowed projection selects data. It is not `view MenuCourse`.
export struct PublicCourse {
  let title: ref String
  let allergens: view Array<String>
}

export fn publicCourse(course: ref MenuCourse): PublicCourse {
  return PublicCourse(
    title: ref course.title,
    allergens: course.allergens[0..<course.allergens.count],
  )
}

export fn serviceTemperatures(
  values: ref Array<f64>,
): view Array<f64> {
  guard values.count >= 2 else return values[0..<0]
  return values[1..<values.count]
}

export fn correctTemperatures(
  values: mut view Array<f64>,
  offset correction: f64,
): mut view Array<f64> {
  let mut view corrected: Array<f64> = values[1..<values.count]

  for mut ref temperature in corrected {
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
  let service = serviceTemperatures(values: readings)

  expect service == [42.0, 273.15, 0.0]
  expect service.count == 3

  // Compile-fail assay: a view has no allocator or capacity.
  // print(service.capacity)

  // Compile-fail assay: append can move storage borrowed by service.
  // readings.append(1.0)
  print(service[0])
}

test "a nominal borrowed projection omits private course data" for publicCourse {
  var course = MenuCourse(
    title: "Pan-Galactic broth",
    allergens: ["celery", "nebula dust"],
    supplierContract: "Megadodo confidential",
  )
  let card = publicCourse(course: course)

  expect card.title == "Pan-Galactic broth"
  expect card.allergens == ["celery", "nebula dust"]

  // Compile-fail assay: the omitted field is not part of PublicCourse.
  // print(card.supplierContract)

  // Compile-fail assay: card fields keep their source places borrowed.
  // course.title.append(" encore")
  print(card.title)
}

test "an exclusive view mutates elements but not its extent" for correctTemperatures {
  var readings = [2.70, 41.5, 272.65]
  let mut view corrected = correctTemperatures(values: mut view readings, offset: 0.5)

  expect corrected == [42.0, 273.15]
  expect readings == [2.70, 42.0, 273.15]

  // Compile-fail assay: a mut view cannot resize its owner.
  // corrected.append(0.0)
}

test "a string view validates text boundaries and materializes explicitly" for commandVerb {
  let command = "serve horizon-cake"
  guard let verb = commandVerb(line: command) else panic("verb is missing")

  expect verb == "serve"
  let owned = verb.materialize()
  expect owned == "serve"

  // Compile-fail assay: String mutation waits for the borrowed view to end.
  // command.append(" now")
  print(verb)
}
