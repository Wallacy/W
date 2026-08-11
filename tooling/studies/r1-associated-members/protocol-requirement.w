// R1 Last Light associated-member study variant.

protocol CourseFactory {
  const count: usize
  static fn fromOrdinal(value: usize): Self?
}

enum Course: CourseFactory {
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

fn selectCourse<T: CourseFactory>(value: usize): T? {
  guard value < T.count else return .none
  return T.fromOrdinal(value)
}

test "protocol requirements select the same course" for selectCourse {
  expect Course.count == 4
  expect selectCourse<Course>(0) == .some(.nebulaBroth)
  expect selectCourse<Course>(3) == .some(.horizonCake)
  expect selectCourse<Course>(4) == .none
}
