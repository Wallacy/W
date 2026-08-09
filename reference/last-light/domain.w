// Core values for the Last Light restaurant.

export type GuestId = u64
export type OrderId = u64
export type GuestCount = u16<(1...4096)>
export type Probability = f64<(0.0...1.0)>
export type BasisPoints = u16<(0...10_000)>
export struct TextBounds {
  min: usize
  max: usize
}

export type BoundedText<_ bounds: TextBounds> =
  String<(.scalars.count in bounds.min...bounds.max)>
export type GuestName = BoundedText<{min: 1, max: 120}>
export type DishLabel = BoundedText<{min: 1, max: 80}>

export enum Course {
  nebulaBroth
  photonSouffle
  quietSalad
  horizonCake

  export static fn fromOrdinal(value: usize): Course {
    return switch value {
      case 0: .nebulaBroth
      case 1: .photonSouffle
      case 2: .quietSalad
      case 3: .horizonCake
      case _: panic("Course ordinal outside the closed enum")
    }
  }
}

export const fn courseLabel(course: Course): DishLabel {
  return switch course {
    case .nebulaBroth: "Nebula broth"
    case .photonSouffle: "Photon souffle"
    case .quietSalad: "Quiet salad"
    case .horizonCake: "Horizon cake"
  }
}

export enum SimulationProfile {
  quietOrbit
  photonRush
  timelineCollision
}

export enum ServiceStage {
  accepted
  reserving
  preparing
  serving
  completed
  cancelled
}

export alias CancelledStage =
  ServiceStage<[.cancelled]>

export const fn canMove(from current: ServiceStage, to next: ServiceStage): Bool {
  return switch current {
    case .accepted: next in (.reserving, .cancelled)
    case .reserving: next in (.preparing, .cancelled)
    case .preparing: next in (.serving, .cancelled)
    case .serving: next in (.completed, .cancelled)
    case .completed: false
    case .cancelled: false
  }
}

export const fn isValidStagePath(stages: StaticList<ServiceStage>): Bool {
  guard stages.count > 0 else return false

  for index in 1..<stages.count {
    if !canMove(from: stages[index - 1], to: stages[index]) {
      return false
    }
  }

  return true
}

export enum PartySize {
  intimate
  regular
  cosmic
}

export fn classifyParty(
  stage: ServiceStage,
  guests: GuestCount,
): PartySize {
  return switch (stage, guests) {
    case (.accepted, 1...4): .intimate
    case (.accepted, 5...20): .regular
    case (.accepted, _): .cosmic
    case (_, _) if guests > 1_000: .cosmic
    case (_, _): .regular
  }
}

export struct StagePath<
  _ stages: StaticList<ServiceStage><(isValidStagePath(.member))>,
> {
  orderId: OrderId
}

export fn standardStagePath(
  orderId: OrderId,
): StagePath<[.accepted, .reserving, .preparing, .serving, .completed]> {
  return StagePath(orderId: orderId)
}

export struct Guest {
  id: GuestId
  name: GuestName
}

export struct Order {
  id: OrderId
  guest: Guest
  guests: GuestCount
  course: Course
  notes: String?
  timeline: u32 = 0
}

export struct Dish {
  orderId: OrderId
  course: Course
  label: DishLabel
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

export const fn currencyCode(currency: Currency): String {
  return switch currency {
    case .cr: "CR"
    case .usd: "USD"
    case .brl: "BRL"
  }
}

export struct Money {
  export minorUnits: i128
  export currency: Currency

  export const zeroCredits = Money(minorUnits: 0, currency: .cr)

  export const init(minorUnits: i128, currency: Currency) {
    self.minorUnits = minorUnits
    self.currency = currency
  }

  export init(majorUnits: i64, currency: Currency) throws DomainError {
    let minorUnits = try i128.checkedMultiply(i128(majorUnits), 100)
      .mapError((_) => .overflow)

    self = Money(minorUnits: minorUnits, currency: currency)
  }
}

export enum DomainError: Error {
  invalidGuestCount(RefinementError)
  invalidTransition(from: ServiceStage, to: ServiceStage)
  unknownOrder(OrderId)
  currencyMismatch(expected: Currency, found: Currency)
  overflow
}

export fn add(left: Money, to right: Money): Money throws DomainError {
  guard left.currency == right.currency else {
    throw .currencyMismatch(expected: left.currency, found: right.currency)
  }

  let total = try i128.checkedAdd(left.minorUnits, right.minorUnits)
    .mapError((_) => .overflow)

  return Money(minorUnits: total, currency: left.currency)
}

test "tuple patterns classify a party in lexical order" for classifyParty {
  expect classifyParty(.accepted, guests: try GuestCount(2)) == .intimate
  expect classifyParty(.serving, guests: try GuestCount(2_000)) == .cosmic
}
