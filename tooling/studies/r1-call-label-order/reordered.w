// R1 Last Light call-label-order study variant.

enum Currency {
  cr
}

struct Money {
  minorUnits: i64
  currency: Currency

  init(majorUnits: i64, currency: Currency) {
    self.minorUnits = majorUnits * 100
    self.currency = currency
  }
}

fn price(): Money {
  return Money(currency: .cr, majorUnits: 42)
}

test "reordered labels keep the Money outcome" for price {
  let value = price()
  expect value.minorUnits == 4_200
  expect value.currency == .cr
}
