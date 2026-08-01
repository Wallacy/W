// Durable fulfillment with explicit points, effects, timers, and events.

import {
  BillingApi,
  Payment,
  PaymentProof,
  billing,
  loadPriceTable,
  paymentKey,
  quote,
  refundKey,
  servingProof,
} from restaurant.billing
import {
  Dish,
  Order,
  Receipt,
  ServiceStage,
} from restaurant.domain
import {
  DiningRoomApi,
  DiningRoomError,
  TableId,
  diningRoom,
} from restaurant.dining
import { AromaProbeApi, aromaProbe } from restaurant.hardware
import { OvenApi, PantryApi, ovens, pantry } from restaurant.kitchen
import { OracleApi, oracle } from restaurant.oracle
import { RestaurantError, prepareDish } from restaurant.restaurant

export struct FulfillmentInput {
  order: Order
}

export enum FulfillmentPoint {
  prepareDish
  settleProbability
  awaitTable
  capturePayment
  serveDish
  refundPayment
}

export enum FulfillmentSignal {
  tableReady(TableId)
  restaurantClosing
}

export const fulfillmentSignals =
  WorkEventBinding<FulfillmentSignal>(name: "fulfillment", version: 1)

export struct CapturedDish {
  dish: Dish
  payment: Payment
}

struct ServingInput {
  tableId: TableId
  dish: Dish
  payment: PaymentProof
}

const fn shouldRetryPayment(error: ref RestaurantError): Bool {
  return switch error {
    case .billing(.gatewayUnavailable): true
    case _: false
  }
}

const paymentRetry = StepRetry<RestaurantError>(
  maximumAttempts: 4,
  backoff: .exponential(
    initial: 1<si.s>,
    factor: 2,
    maximum: 8<si.s>,
  ),
  attemptTimeout: .some(10<si.s>),
  retryWhen: shouldRetryPayment,
)

package async fn prepareDishStep(
  input: take FulfillmentInput,
  step: StepContext,
): Dish throws RestaurantError {
  let pantryRef = try await step.services.get(pantry)
  let ovenRefs = try await step.services.get(ovens)
  let oracleRef = try await step.services.get(oracle)
  let probe = try await step.services.get(aromaProbe)

  return try await prepareDish(
    take input.order,
    pantry: pantryRef,
    ovens: ovenRefs,
    oracle: oracleRef,
    probe: probe,
  )
}

package async fn capturePaymentStep(
  dish: take Dish,
  step: StepContext,
): CapturedDish throws RestaurantError {
  let billingRef = try await step.services.get(billing)
  let amount = try quote(loadPriceTable(), course: dish.course)
  let key = paymentKey(dish.orderId)
  let payment = try await billingRef.capture(amount, idempotencyKey: key)
  return CapturedDish(dish: take dish, payment: take payment)
}

package async fn serveDishStep(
  input: take ServingInput,
  step: StepContext,
): Receipt throws RestaurantError {
  let diningRoomRef = try await step.services.get(diningRoom)
  return try await diningRoomRef.serve(
    at: input.tableId,
    dish: take input.dish,
    payment: input.payment,
  )
}

package async fn refundPaymentStep(
  payment: take Payment,
  step: StepContext,
): Payment throws RestaurantError {
  let billingRef = try await step.services.get(billing)
  let key = refundKey(payment.id)
  return try await billingRef.refund(
    take payment,
    idempotencyKey: key,
  )
}

package async fn fulfillOrderDurably(
  input: take FulfillmentInput,
  work: WorkContext<ServiceStage>,
): Receipt throws RestaurantError {
  let dish = try await work.step(
    .prepareDish,
    progress: .reserving,
    succeeded: .some(.preparing),
    input: take input,
    effect: .atMostOnce,
    using: prepareDishStep,
  )

  try await work.sleep(.settleProbability, for: 2<si.s>)

  let table = try await work.wait(
    .awaitTable,
    for: fulfillmentSignals,
    timeout: 1_800<si.s>,
  )

  let tableId = switch table {
    case .event(.tableReady(let tableId)): tableId
    case .event(.restaurantClosing): throw .dining(.full)
    case .timeout: throw .dining(.full)
  }

  let captured = try await work.step(
    .capturePayment,
    progress: .serving,
    input: take dish,
    effect: .idempotent,
    retry: paymentRetry,
    using: capturePaymentStep,
  )

  let CapturedDish(dish, payment) = take captured
  let proof = servingProof(payment)
  let serving = ServingInput(
    tableId: tableId,
    dish: take dish,
    payment: proof,
  )

  do {
    return try await work.step(
      .serveDish,
      progress: .serving,
      succeeded: .some(.completed),
      input: take serving,
      effect: .atMostOnce,
      using: serveDishStep,
    )
  } catch error {
    let _ = try await work.step(
      .refundPayment,
      progress: .serving,
      input: take payment,
      effect: .idempotent,
      retry: paymentRetry,
      using: refundPaymentStep,
    )
    throw error
  }
}

test "fulfillment signals keep closing distinct from table admission" {
  let ready: FulfillmentSignal = .tableReady(42)
  let closing: FulfillmentSignal = .restaurantClosing
  expect ready != closing
}

// Compile-fail assays:
// let now = Clock.now() // Clock observations must occur inside a step.
// let pantryRef = try await work.services.get(pantry) // Direct workflow I/O.
// try await work.sleep(.awaitTable, for: 1<si.s>) // Point already used by wait.
