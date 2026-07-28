// Pricing, idempotency, and compensation for the final service window.

import { Course, Currency, DomainError, Money, OrderId } from restaurant.domain

export type PaymentId = u64
export type IdempotencyKey = String where (value.scalars.count in 8...128)

export enum PaymentState {
  authorized
  captured
  refunded
  unknown
}

export struct Payment {
  id: PaymentId
  key: IdempotencyKey
  amount: Money
  state: PaymentState
}

export enum BillingError: Error {
  domain(DomainError)
  missingPrice(Course)
  duplicateKey(IdempotencyKey)
  gatewayUnavailable
  unknownOutcome(PaymentId)
  invalidTransition(from: PaymentState, to: PaymentState)
}

export behavior Versioned<Value> for Value {
  storage var current: Value
  storage var revision: u64 = 0
  initialValue

  init {
    current = initialValue()
  }

  get {
    return current
  }

  mut set(newValue) {
    current = newValue
    revision += 1
  }
}

export protocol PricingPolicy {
  fn price(for course: Course): Money throws BillingError
}

export struct PriceTable {
  prices: Map<Course, Money>
}

extension PriceTable: PricingPolicy {
  fn price(for course: Course): Money throws BillingError {
    guard let price = prices[course] else throw .missingPrice(course)
    return price
  }
}

export fn loadPriceTable(): PriceTable {
  return PriceTable(prices: [
    .nebulaBroth: Money(minorUnits: 1_200, currency: .cr),
    .photonSouffle: Money(minorUnits: 1_900, currency: .cr),
    .quietSalad: Money(minorUnits: 900, currency: .cr),
    .horizonCake: Money(minorUnits: 4_242, currency: .cr),
  ])
}

export fn activePricingPolicy(table: take PriceTable): some PricingPolicy {
  return table
}

export fn quote(policy: ref any PricingPolicy, course: Course): Money throws BillingError {
  return try policy.price(for: course)
}

export fn paymentKey(orderId: OrderId): IdempotencyKey {
  return try IdempotencyKey("order:${orderId}:capture")
}

export fn refundKey(paymentId: PaymentId): IdempotencyKey {
  return try IdempotencyKey("payment:${paymentId}:refund")
}

export protocol PaymentGatewayApi {
  async fn capture(amount: Money, idempotencyKey: IdempotencyKey): Payment throws BillingError
  async fn refund(payment: ref Payment, idempotencyKey: IdempotencyKey): Payment throws BillingError
}

export protocol BillingApi {
  async fn capture(amount: Money, idempotencyKey: IdempotencyKey): Payment throws BillingError
  async fn refund(payment: ref Payment, idempotencyKey: IdempotencyKey): Payment throws BillingError
}

export service BillingLedger as BillingApi {
  gateway: ServiceRef<PaymentGatewayApi>
  var Versioned payments: Map<IdempotencyKey, Payment> = Map()

  mut async fn capture(amount: Money, idempotencyKey key: IdempotencyKey): Payment throws BillingError {
    if let payment = payments[key] {
      guard payment.amount == amount else throw .duplicateKey(key)
      return payment
    }

    let payment = try await gateway.capture(amount, idempotencyKey: key)
    payments[key] = payment
    return payment
  }

  mut async fn refund(payment: ref Payment, idempotencyKey key: IdempotencyKey): Payment throws BillingError {
    if let prior = payments[key] {
      if prior.state == .refunded {
        return prior
      }
    }

    let refunded = try await gateway.refund(payment, idempotencyKey: key)
    payments[key] = refunded
    return refunded
  }
}

test "pricing keeps currency and integer minor units" for quote {
  let table = loadPriceTable()
  let policy: any PricingPolicy = table
  let value = try quote(policy, course: .horizonCake)

  expect value.currency == Currency.cr
  expect value.minorUnits == 4_242
}
