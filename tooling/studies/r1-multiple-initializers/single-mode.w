// R1 Last Light initializer study: one initializer with a mode value.

enum Currency { cr }

enum MoneyInput {
  minorUnits(i128)
  majorUnits(i64)
}

struct Money {
  minorUnits: i128
  currency: Currency

  const init(value input: MoneyInput, currency: Currency) {
    self.minorUnits = switch input {
      case .minorUnits(let value): value
      case .majorUnits(let value): i128(value) * 100
    }
    self.currency = currency
  }
}

fn exactPrice(value: i128): Money {
  return Money(value: .minorUnits(value), currency: .ww)
}

fn majorPrice(value: i64): Money {
  return Money(value: .majorUnits(value), currency: .ww)
}

test "one mode initializer constructs the same value" for majorPrice {
  expect exactPrice(4_200).minorUnits == 4_200
  expect majorPrice(42).minorUnits == 4_200
}
