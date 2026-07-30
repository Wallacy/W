// Short coordination turns and runtime-owned fulfillment work.

import {
  Order,
  OrderId,
  Receipt,
  ServiceStage,
} from restaurant.domain
import {
  BillingApi,
  loadPriceTable,
  paymentKey,
  quote,
  refundKey,
  servingProof,
} from restaurant.billing
import { DiningRoomApi } from restaurant.dining
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

export alias FulfillmentSupervisor = SupervisorRef<OrderId, FulfillmentInput, ServiceStage, Receipt, RestaurantError>
export alias FulfillmentKey = WorkKeyRef<OrderId, FulfillmentInput, ServiceStage, Receipt, RestaurantError>
export alias FulfillmentSnapshot = WorkSnapshot<ServiceStage>

export struct OrderAccepted {
  orderId: OrderId
  workId: WorkId
  revision: u64
}

export enum CoordinatorError: Error {
  start(WorkStartError<OrderId, FulfillmentInput>)
  lookup(WorkLookupError)
  supervisor(SupervisorFailure)
  wrongInstance(expected: OrderId, found: OrderId)
}

package async fn fulfillOrder(
  input: take FulfillmentInput,
  work: WorkContext<ServiceStage>,
): Receipt throws RestaurantError {
  let orderId = input.order.id
  work.report(.accepted)
  Task.checkCancellation()

  let pantry = try await work.services.get(pantryService)
  let ovens = try await work.services.get(ovenService)
  let oracle = try await work.services.get(oracleService)
  let probe = try await work.services.get(aromaProbeService)
  let billing = try await work.services.get(billingService)
  let diningRoom = try await work.services.get(diningRoomService)

  work.report(.reserving)
  Task.checkCancellation()
  work.report(.preparing)

  let dish = try await prepareDish(take input.order, pantry: pantry, ovens: ovens, oracle: oracle, probe: probe)

  work.report(.serving)
  let priceTable = loadPriceTable()
  let amount = try quote(priceTable, course: dish.course)
  let payment = try await billing.capture(amount, idempotencyKey: paymentKey(orderId))
  let proof = servingProof(payment)
  let refundIdempotencyKey = refundKey(payment.id)
  var completed = false

  defer async {
    if !completed {
      do {
        let _ = try await billing.refund(take payment, idempotencyKey: refundIdempotencyKey)
      } catch error {
        Trace.current.recordCleanupError(error)
      }
    }
  }

  let receipt = try await diningRoom.serve(take dish, payment: proof)
  completed = true
  work.report(.completed)
  return receipt
}

export protocol OrderCoordinatorApi {
  async fn submit(order: take Order): OrderAccepted throws CoordinatorError
  async fn status(): FulfillmentSnapshot throws CoordinatorError
  async fn cancel(): WorkCancelResult<ServiceStage> throws CoordinatorError
  async fn outcome(): WorkOutcome<Receipt, RestaurantError>? throws CoordinatorError
}

export const orderCoordinators = ServiceFamily<OrderCoordinatorApi, OrderId>(name: "orders")

export service OrderCoordinator as OrderCoordinatorApi {
  identity: ServiceIdentity<OrderId>
  fulfillment: FulfillmentKey

  fn requireInstance(orderId: OrderId) throws CoordinatorError {
    guard orderId == identity.key else {
      throw .wrongInstance(expected: identity.key, found: orderId)
    }
  }

  async fn submit(order: take Order): OrderAccepted throws CoordinatorError {
    let orderId = order.id
    try requireInstance(orderId)
    let input = FulfillmentInput(order: take order)
    let started = try await fulfillment.tryStart(input: take input)
    return OrderAccepted(orderId: orderId, workId: started.id, revision: started.revision)
  }

  async fn status(): FulfillmentSnapshot throws CoordinatorError {
    return try await fulfillment.snapshot()
  }

  async fn cancel(): WorkCancelResult<ServiceStage> throws CoordinatorError {
    return try await fulfillment.cancel(reason: .userRequest)
  }

  async fn outcome(): WorkOutcome<Receipt, RestaurantError>? throws CoordinatorError {
    return try await fulfillment.outcome()
  }
}

const fn isPreparing(state: WorkState, progress: ServiceStage?): Bool {
  return state == .running && progress == .some(.preparing)
}

test "supervised progress keeps the domain stage separate from work state" {
  expect isPreparing(.running, progress: .some(.preparing))
  expect !isPreparing(.succeeded, progress: .some(.preparing))
}
