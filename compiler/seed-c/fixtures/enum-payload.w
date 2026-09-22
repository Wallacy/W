enum Course {
  starter
  main(price: i64, tax: i64)
  dessert(price: i64)
}

fn order(price: i64, tax: i64): Course {
  return .main(tax: tax, price: price)
}

fn bill(course: Course): i64 {
  return switch course {
    case .dessert(price: let amount): amount
    case .starter: 10
    case .main(tax: let fee, price: let amount): amount + fee
  }
}

entry {
  let first = order(price: 30, tax: 2)
  let second = order(price: 41, tax: 3)
  let starter: Course = .starter
  let dessert: Course = .dessert(price: 7)
  let firstBill = bill(course: first)
  let secondBill = bill(course: second)
  let starterBill = bill(course: starter)
  let dessertBill = bill(course: dessert)
  print("Bills ${firstBill}/${secondBill}/${starterBill}/${dessertBill}")
}
