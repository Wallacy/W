enum Course {
  starter
  main
  dessert
}

fn price(course: Course): i64 {
  return switch course {
    case .starter: 10
    case .main: 30
    case .dessert: 20
  }
}

entry {
  let starter = price(course: .starter)
  let main = price(course: .main)
  let dessert = price(course: .dessert)
  print("Courses ${starter}/${main}/${dessert}")
}
