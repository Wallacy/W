// R1 Last Light initializer study: disjoint initializer shapes.

enum Currency { cr }

struct Money {
  minorUnits: i128
  currency: Currency

  const init(minorUnits value: i128, currency: Currency) {
    self.minorUnits = value
    self.currency = currency
  }

  const init(majorUnits value: i64, currency: Currency) {
    self = Money(minorUnits: i128(value) * 100, currency: currency)
  }
}

fn exactPrice(_ value: i128): Money {
  return Money(minorUnits: value, currency: .ww)
}

fn majorPrice(_ value: i64): Money {
  return Money(majorUnits: value, currency: .ww)
}

test "disjoint initializer shapes construct the same value" for majorPrice {
  expect exactPrice(4_200).minorUnits == 4_200
  expect majorPrice(42).minorUnits == 4_200
}
