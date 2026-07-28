// Core values for the Last Light restaurant.

export type GuestId = u64
export type OrderId = u64
export type GuestCount = u16 where (value in 1...4096)
export type Probability = f64 where (value in 0.0...1.0)
export type BasisPoints = u16 where (value in 0...10_000)

export enum Course {
  nebulaBroth
  photonSouffle
  quietSalad
  horizonCake
}

export enum ServiceStage {
  accepted
  reserving
  preparing
  serving
  completed
  cancelled
}

export struct Guest {
  id: GuestId
  name: String
}

export struct Order {
  id: OrderId
  guest: Guest
  guests: GuestCount
  course: Course
  notes: String?
}

export struct Dish {
  orderId: OrderId
  course: Course
  label: String
}

export struct Receipt {
  orderId: OrderId
  total: Money
  traceId: TraceId
}

export enum Currency {
  cr
  usd
  brl
}

export struct Money {
  minorUnits: i128
  currency: Currency
}

export enum DomainError: Error {
  invalidGuestCount(RefinementError)
  invalidTransition(from: ServiceStage, to: ServiceStage)
  unknownOrder(OrderId)
  currencyMismatch(expected: Currency, found: Currency)
  overflow
}

export fn canMove(from current: ServiceStage, to next: ServiceStage): Bool {
  return switch current {
    case .accepted: next in (.reserving, .cancelled)
    case .reserving: next in (.preparing, .cancelled)
    case .preparing: next in (.serving, .cancelled)
    case .serving: next in (.completed, .cancelled)
    case .completed: false
    case .cancelled: false
  }
}

export fn add(left: Money, to right: Money): Money throws DomainError {
  guard left.currency == right.currency else {
    throw .currencyMismatch(expected: left.currency, found: right.currency)
  }

  let total = try i128.checkedAdd(left.minorUnits, right.minorUnits)
    .mapError((_) => .overflow)

  return Money(minorUnits: total, currency: left.currency)
}
