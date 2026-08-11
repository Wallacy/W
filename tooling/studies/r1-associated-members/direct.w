// R1 Last Light associated-member study variant.

enum Course {
  nebulaBroth
  photonSouffle
  quietSalad
  horizonCake

  const count: usize = 4

  static fn fromOrdinal(value: usize): Course? {
    return switch value {
      case 0: .some(.nebulaBroth)
      case 1: .some(.photonSouffle)
      case 2: .some(.quietSalad)
      case 3: .some(.horizonCake)
      case _: .none
    }
  }
}

fn selectCourse(value: usize): Course? {
  guard value < Course.count else return .none
  return Course.fromOrdinal(value)
}

test "direct associated members select a course" for selectCourse {
  expect Course.count == 4
  expect selectCourse(0) == .some(.nebulaBroth)
  expect selectCourse(3) == .some(.horizonCake)
  expect selectCourse(4) == .none
}
