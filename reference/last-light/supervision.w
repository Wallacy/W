// Short coordination turns and runtime-owned fulfillment work.

import {
  Order,
  OrderId,
  Receipt,
  ServiceStage,
} from domain
import {
  BillingApi,
  billing,
  loadPriceTable,
  paymentKey,
  quote,
  refundKey,
  servingProof,
} from billing
import { DiningRoomApi, diningRoom } from dining
import { AromaProbeApi, aromaProbe } from hardware
import { OvenApi, PantryApi } from kitchen
import service {
  OvenApi as ovens,
  PantryApi as pantry,
} from kitchen
import { OracleApi, oracle } from oracle
import { RestaurantError, prepareDish } from restaurant
import {
  FulfillmentInput,
  FulfillmentSignal,
  fulfillmentSignals,
} from workflow

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
  event(WorkEventSendError<FulfillmentSignal>)
  lookup(WorkLookupError)
  supervisor(SupervisorFailure)
  wrongInstance(expected: OrderId, found: OrderId)
}

// Process-local compensation oracle. The product binds fulfillOrderDurably.
async fn fulfillOrder(
  input: take FulfillmentInput,
  work: WorkContext<ServiceStage>,
): Receipt throws RestaurantError {
  let orderId = input.order.id
  work.report(.accepted)
  execution#checkCancellation()

  work.report(.reserving)
  execution#checkCancellation()
  work.report(.preparing)

  let dish = try await prepareDish(
    order: take input.order,
    pantry: pantry,
    ovens: ovens,
    oracle: oracle,
    probe: aromaProbe,
  )

  work.report(.serving)
  let priceTable = loadPriceTable()
  let amount = try quote(policy: priceTable, course: dish.course)
  let payment = try await billing.capture(amount, idempotencyKey: paymentKey(orderId: orderId))
  let proof = servingProof(payment: payment)
  let refundIdempotencyKey = refundKey(paymentId: payment.id)
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
  async fn signal(id: EventId, event: take FulfillmentSignal): WorkEventSendResult throws CoordinatorError
  async fn status(): FulfillmentSnapshot throws CoordinatorError
  async fn cancel(): WorkCancelResult<ServiceStage> throws CoordinatorError
  async fn outcome(): WorkOutcome<Receipt, RestaurantError>? throws CoordinatorError
}

export service orderCoordinators<key: OrderId>: OrderCoordinatorApi {
  identity: ServiceIdentity<OrderId>
  fulfillment: FulfillmentKey

  init(
    identity: ServiceIdentity<OrderId>,
    fulfillment: FulfillmentKey,
  ) {
    self.identity = identity
    self.fulfillment = fulfillment
  }

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

  async fn signal(
    id: EventId,
    event: take FulfillmentSignal,
  ): WorkEventSendResult throws CoordinatorError {
    return try await fulfillment.trySend(
      fulfillmentSignals,
      id: id,
      payload: take event,
    )
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
  expect isPreparing(state: .running, progress: .some(.preparing))
  expect !isPreparing(state: .succeeded, progress: .some(.preparing))
}
