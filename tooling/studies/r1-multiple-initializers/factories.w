// R1 Last Light initializer study: canonical initializer plus a factory.

enum Currency { cr }

struct Money {
  minorUnits: i128
  currency: Currency

  const init(minorUnits value: i128, currency: Currency) {
    self.minorUnits = value
    self.currency = currency
  }

  static const fn fromMajorUnits(value: i64, currency: Currency): Money {
    return Money(minorUnits: i128(value) * 100, currency: currency)
  }
}

fn exactPrice(value: i128): Money {
  return Money(minorUnits: value, currency: .ww)
}

fn majorPrice(value: i64): Money {
  return Money.fromMajorUnits(value, currency: .ww)
}

test "a factory constructs the same value" for majorPrice {
  expect exactPrice(4_200).minorUnits == 4_200
  expect majorPrice(42).minorUnits == 4_200
}
