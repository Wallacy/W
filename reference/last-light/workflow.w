// Durable fulfillment with explicit points, effects, timers, and events.

import {
  BillingApi,
  Payment,
  PaymentProof,
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
} from restaurant.dining
import { AromaProbeApi } from restaurant.hardware
import { OvenApi, PantryApi } from restaurant.kitchen
import { OracleApi } from restaurant.oracle
import { RestaurantError, prepareDish } from restaurant.restaurant

const pantryService = ServiceBinding<PantryApi>(name: "pantry")
const ovenService = ServiceBinding<OvenApi>(name: "ovens")
const oracleService = ServiceBinding<OracleApi>(name: "oracle")
const aromaProbeService = ServiceBinding<AromaProbeApi>(name: "aroma-probe")
const billingService = ServiceBinding<BillingApi>(name: "billing")
const diningRoomService = ServiceBinding<DiningRoomApi>(name: "dining-room")

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
  let pantry = try await step.services.get(pantryService)
  let ovens = try await step.services.get(ovenService)
  let oracle = try await step.services.get(oracleService)
  let probe = try await step.services.get(aromaProbeService)

  return try await prepareDish(
    take input.order,
    pantry: pantry,
    ovens: ovens,
    oracle: oracle,
    probe: probe,
  )
}

package async fn capturePaymentStep(
  dish: take Dish,
  step: StepContext,
): CapturedDish throws RestaurantError {
  let billing = try await step.services.get(billingService)
  let amount = try quote(loadPriceTable(), course: dish.course)
  let key = paymentKey(dish.orderId)
  let payment = try await billing.capture(amount, idempotencyKey: key)
  return CapturedDish(dish: take dish, payment: take payment)
}

package async fn serveDishStep(
  input: take ServingInput,
  step: StepContext,
): Receipt throws RestaurantError {
  let diningRoom = try await step.services.get(diningRoomService)
  return try await diningRoom.serve(
    at: input.tableId,
    dish: take input.dish,
    payment: input.payment,
  )
}

package async fn refundPaymentStep(
  payment: take Payment,
  step: StepContext,
): Payment throws RestaurantError {
  let billing = try await step.services.get(billingService)
  let key = refundKey(payment.id)
  return try await billing.refund(
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
// let pantry = try await work.services.get(pantryService) // Direct workflow I/O.
// try await work.sleep(.awaitTable, for: 1<si.s>) // Point already used by wait.
