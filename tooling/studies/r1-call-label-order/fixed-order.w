// R1 Last Light call-label-order study variant.

enum Currency {
  cr
}

struct Money {
  minorUnits: i64
  currency: Currency

  init(majorUnits value: i64, currency: Currency) {
    self.minorUnits = value * 100
    self.currency = currency
  }
}

fn price(): Money {
  return Money(majorUnits: 42, currency: .ww)
}

test "fixed declaration order keeps the Money outcome" for price {
  let value = price()
  expect value.minorUnits == 4_200
  expect value.currency == .ww
}
