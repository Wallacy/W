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
} from billing
import {
  Dish,
  Order,
  Receipt,
  ServiceStage,
} from domain
import {
  DiningRoomApi,
  DiningRoomError,
  TableId,
  diningRoom,
} from dining
import { AromaProbeApi, aromaProbe } from hardware
import { OvenApi, PantryApi } from kitchen
import service {
  OvenApi as ovens,
  PantryApi as pantry,
} from kitchen
import { OracleApi, oracle } from oracle
import { RestaurantError, prepareDish } from restaurant

export struct FulfillmentInput {
  let order: Order
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

// W-1241: another durable root is an explicit call, not inherited child state.
enum DurableComposition {
  currentStep
  serviceRoot
  supervisorRoot
  childIntrinsic
  continueAsNewIntrinsic
}

const fn usesDurableBaseline(_ composition: DurableComposition): Bool {
  return composition.one(.currentStep, .serviceRoot, .supervisorRoot)
}

export struct CapturedDish {
  let dish: Dish
  let payment: Payment
}

struct ServingInput {
  let tableId: TableId
  let dish: Dish
  let payment: PaymentProof
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

async fn prepareDishStep(
  input: take FulfillmentInput,
  step: StepContext,
): Dish throws RestaurantError {
  return try await prepareDish(
    order: take input.order,
    pantry: pantry,
    ovens: ovens,
    oracle: oracle,
    probe: aromaProbe,
  )
}

async fn capturePaymentStep(
  dish: take Dish,
  step: StepContext,
): CapturedDish throws RestaurantError {
  let amount = try quote(policy: loadPriceTable(), course: dish.course)
  let key = paymentKey(orderId: dish.orderId)
  let payment = try await billing.capture(amount, idempotencyKey: key)
  return CapturedDish(dish: take dish, payment: take payment)
}

async fn serveDishStep(
  input: take ServingInput,
  step: StepContext,
): Receipt throws RestaurantError {
  return try await diningRoom.serve(
    at: input.tableId,
    dish: take input.dish,
    payment: input.payment,
  )
}

async fn refundPaymentStep(
  payment: take Payment,
  step: StepContext,
): Payment throws RestaurantError {
  let key = refundKey(paymentId: payment.id)
  return try await billing.refund(
    take payment,
    idempotencyKey: key,
  )
}

async fn fulfillOrderDurably(
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
  let proof = servingProof(payment: payment)
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

test "durable composition keeps each root explicit" for usesDurableBaseline {
  expect usesDurableBaseline(.currentStep)
  expect usesDurableBaseline(.serviceRoot)
  expect usesDurableBaseline(.supervisorRoot)
  expect !usesDurableBaseline(.childIntrinsic)
  expect !usesDurableBaseline(.continueAsNewIntrinsic)
}

// Compile-fail assays:
// let now = Clock.now() // Clock observations must occur inside a step.
// let stock = try await pantry.reserve(course, guests: guests) // Direct workflow I/O.
// try await work.sleep(.awaitTable, for: 1<si.s>) // Point already used by wait.
