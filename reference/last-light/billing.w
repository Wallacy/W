// Pricing, idempotency, and compensation for the final service window.

import {
  Course,
  Currency,
  DishLabel,
  DomainError,
  Money,
  OrderId,
  courseLabel,
} from domain

export type PaymentId = u64
export type IdempotencyKey = String<(.scalars.count in 8...128)>

export enum PaymentState {
  authorized
  captured
  refunded
  unknown
}

export struct Payment {
  let id: PaymentId
  let amount: Money
  let state: PaymentState
}

export struct PaymentProof {
  let paymentId: PaymentId
  let amount: Money
  let state: PaymentState

  export let canServe: Bool {
    get => state == .captured
  }
}

export struct MenuItem {
  let course: Course
  let label: DishLabel
  let price: Money
}

export enum BillingError: Error {
  domain(DomainError)
  missingPrice(Course)
  duplicateKey(IdempotencyKey)
  gatewayUnavailable
  unknownOutcome(PaymentId)
  invalidTransition(from: PaymentState, to: PaymentState)
  service(ServiceFailure)
}

export protocol PricingPolicy {
  fn price(for course: Course): Money throws BillingError
}

export struct PriceTable {
  let prices: Map<Course, Money>
}

extension PriceTable: PricingPolicy {
  fn price(for course: Course): Money throws BillingError {
    guard let price = prices[course] else throw .missingPrice(course)
    return price
  }
}

export fn loadPriceTable(): PriceTable {
  return PriceTable(prices: [
    .nebulaBroth: Money(minorUnits: 1_200, currency: .ww),
    .photonSouffle: Money(minorUnits: 1_900, currency: .ww),
    .quietSalad: Money(minorUnits: 900, currency: .ww),
    .horizonCake: Money(minorUnits: 4_242, currency: .ww),
  ])
}

export fn menuItems(table: ref PriceTable): Array<MenuItem> throws BillingError {
  var items: Array<MenuItem> = []

  for course in [.nebulaBroth, .photonSouffle, .quietSalad, .horizonCake] {
    items.append(MenuItem(
      course: course,
      label: courseLabel(course: course),
      price: try table.price(for: course),
    ))
  }

  return items
}

export fn activePricingPolicy(table: take PriceTable): some PricingPolicy {
  return table
}

export fn quote(policy: ref any PricingPolicy, course selectedCourse: Course): Money throws BillingError {
  return try policy.price(for: selectedCourse)
}

export fn paymentKey(orderId: OrderId): IdempotencyKey {
  return try IdempotencyKey("order:${orderId}:capture")
}

export fn refundKey(paymentId: PaymentId): IdempotencyKey {
  return try IdempotencyKey("payment:${paymentId}:refund")
}

export fn servingProof(payment: ref Payment): PaymentProof {
  return PaymentProof(paymentId: payment.id, amount: payment.amount, state: payment.state)
}

export protocol PaymentGatewayApi {
  async fn capture(amount: Money, idempotencyKey: IdempotencyKey): Payment throws BillingError
  async fn refund(payment: take Payment, idempotencyKey: IdempotencyKey): Payment throws BillingError
}

export protocol BillingApi {
  async fn capture(amount: Money, idempotencyKey: IdempotencyKey): Payment throws BillingError
  async fn refund(payment: take Payment, idempotencyKey: IdempotencyKey): Payment throws BillingError
}

export service billing: BillingApi {
  let gateway: ServiceRef<PaymentGatewayApi>
  var payments: Map<IdempotencyKey, Payment> = Map()

  init(gateway: ServiceRef<PaymentGatewayApi>) {
    self.gateway = gateway
  }

  mut async fn capture(amount: Money, idempotencyKey key: IdempotencyKey): Payment throws BillingError {
    if let payment = payments[key] {
      guard payment.amount == amount else throw .duplicateKey(key)
      return payment
    }

    let payment = try await gateway.capture(amount, idempotencyKey: copy key)
    payments[key] = payment
    return payment
  }

  mut async fn refund(payment: take Payment, idempotencyKey key: IdempotencyKey): Payment throws BillingError {
    if let prior = payments[key] {
      if prior.state == .refunded {
        return prior
      }
    }

    let refunded = try await gateway.refund(take payment, idempotencyKey: copy key)
    payments[key] = refunded
    return refunded
  }
}

test "pricing keeps currency and integer minor units" for quote {
  let table = loadPriceTable()
  let menu = try menuItems(table: table)
  let policy: any PricingPolicy = table
  let value = try quote(policy: policy, course: .horizonCake)

  expect value.currency == Currency.ww
  expect value.minorUnits == 4_242
  expect Money.zeroCredits.minorUnits == 0
  expect Money(majorUnits: 42, currency: .ww).minorUnits == 4_200
  expect menu.count == 4
  expect menu.last?.label == "Horizon cake"
}
