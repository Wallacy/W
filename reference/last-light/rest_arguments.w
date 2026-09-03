/// Homogeneous rest arguments preserve labels, ownership, and allocation facts.
import { Course } from domain

fn courseLoad(course: Course): u32 {
  return switch course {
    case .nebulaBroth: 3
    case .photonSouffle: 8
    case .quietSalad: 2
    case .horizonCake: 13
  }
}

export fn kitchenLoad(kitchens: u16, courses plannedCourses: Course...): u32 {
  var total: u32 = 0
  for course in plannedCourses {
    total += courseLoad(course: course) * kitchens
  }
  return total
}

export fn announce(_ messages: ref String...): usize {
  var bytes: usize = 0
  for message in messages {
    bytes += message.bytes.count
  }
  return bytes
}

export struct AuditRecord {
  let message: String
}

export fn archive(_ records: take AuditRecord...): usize {
  let count = records.count
  for record in take records {
    let _ = take record
  }
  return count
}

test "rest arguments keep one labeled call shape" for kitchenLoad {
  let direct = kitchenLoad(
    kitchens: 2,
    courses: .nebulaBroth,
    .horizonCake,
  )
  expect direct == 32

  let planned = [.photonSouffle, .quietSalad]
  expect kitchenLoad(kitchens: 3, courses: each planned) == 30

  let estimator: fn(u16, Course...): u32 = kitchenLoad
  expect estimator(1, courses: each planned) == 10
}

test "owned expansion consumes the collection" for archive {
  let pending = [
    AuditRecord(message: "Open the temporal pantry"),
    AuditRecord(message: "Close it before yesterday"),
  ]

  expect archive(each take pending) == 2
}
