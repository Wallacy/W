// R1 Last Light call-label-order study variant.

enum Currency {
  cr
}

struct Money {
  let minorUnits: i64
  let currency: Currency

  init(currency: Currency, majorUnits value: i64) {
    self.minorUnits = value * 100
    self.currency = currency
  }
}

fn price(): Money {
  return Money(currency: .ww, majorUnits: 42)
}

test "reordered labels keep the Money outcome" for price {
  let value = price()
  expect value.minorUnits == 4_200
  expect value.currency == .ww
}
