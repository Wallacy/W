// Integrated service route for the Turno do Horizonte Violeta.

import {
  CancelledStage,
  Dish,
  DomainError,
  Order,
  OrderId,
  Receipt,
  ServiceStage,
  canMove,
} from restaurant.domain
import {
  BillingApi,
  BillingError,
  loadPriceTable,
  paymentKey,
  quote,
  refundKey,
  servingProof,
} from restaurant.billing
import { DiningRoomApi, DiningRoomError } from restaurant.dining
import { AromaProbeApi, ProbeError } from restaurant.hardware
import {
  KitchenError,
  OvenApi,
  OvenError,
  PantryApi,
  PantryError,
  expectedEnergy,
  mix,
} from restaurant.kitchen
import { OracleApi, OracleError, planningRequest } from restaurant.oracle

export enum RestaurantError: Error {
  domain(DomainError)
  oracle(OracleError)
  probe(ProbeError)
  pantry(PantryError)
  oven(OvenError)
  kitchen(KitchenError)
  billing(BillingError)
  dining(DiningRoomError)
  service(ServiceFailure)
}

export protocol RestaurantApi {
  async fn place(order: take Order): Receipt throws RestaurantError
  async fn status(orderId: OrderId): ServiceStage throws RestaurantError
  async fn cancel(orderId: OrderId): CancelledStage throws RestaurantError
}

struct OrderState {
  var stage: ServiceStage
  var receipt: Receipt?

  mut fn advance(to next: ServiceStage): self throws DomainError {
    guard canMove(from: stage, to: next) else {
      throw .invalidTransition(from: stage, to: next)
    }

    stage = next
  }
}

async fn prepareDish(
  order: take Order,
  pantry: ServiceRef<PantryApi>,
  ovens: ServiceRef<OvenApi>,
  oracle: ServiceRef<OracleApi>,
  probe: ServiceRef<AromaProbeApi>,
): Dish throws RestaurantError {
  let planning = planningRequest(order)
  let Order(guests, course, ...) = take order
  async let stock = pantry.reserve(course, guests: guests)
  async let telemetry = ovens.telemetry()
  async let aromaSample = probe.sample()
  async let schedule = oracle.plan(take planning)

  let (stock, telemetry, aromaSample, schedule) = try await (stock, telemetry, aromaSample, schedule)
  defer async {
    do {
      try await (take stock).release()
    } catch error {
      Trace.current.recordCleanupError(error)
    }
  }

  guard aromaSample.aroma >= schedule.minimumAroma else {
    throw .kitchen(.lowAroma(found: aromaSample.aroma, required: schedule.minimumAroma))
  }

  let projectedEnergy = expectedEnergy(telemetry, during: schedule.duration)
  guard projectedEnergy <= schedule.energyBudget else {
    throw .kitchen(.energyBudgetExceeded(found: projectedEnergy, limit: schedule.energyBudget))
  }

  let lease = try await ovens.acquire(schedule.recipe.target, duration: schedule.duration)
  defer async {
    do {
      try await lease.close()
    } catch error {
      Trace.current.recordCleanupError(error)
    }
  }

  async let preheat = lease.preheat()
  spawn<.compute> let mixture = mix(stock.ingredients, recipe: schedule.recipe)

  let ready = try await preheat
  let mixture = try await mixture
  return try await lease.bake(mixture, until: ready.deadline)
}

export service LastLightRestaurant as RestaurantApi {
  pantry: ServiceRef<PantryApi>
  ovens: ServiceRef<OvenApi>
  oracle: ServiceRef<OracleApi>
  probe: ServiceRef<AromaProbeApi>
  billing: ServiceRef<BillingApi>
  diningRoom: ServiceRef<DiningRoomApi>
  var orders: Map<OrderId, OrderState> = Map()
  var Lazy priceTable = loadPriceTable()
  var atomic completedOrders: u64 = 0

  mut async fn place(order: take Order): Receipt throws RestaurantError {
    let orderId = order.id
    var state = OrderState(stage: .accepted, receipt: .none)
    try state.advance(to: .reserving)
    orders[orderId] = state

    try state.advance(to: .preparing)
    orders[orderId] = state

    let dish = try await prepareDish(
      take order,
      pantry: pantry,
      ovens: ovens,
      oracle: oracle,
      probe: probe,
    )

    try state.advance(to: .serving)
    orders[orderId] = state

    let amount = try quote(priceTable, course: dish.course)
    let payment = try await billing.capture(amount, idempotencyKey: paymentKey(orderId))
    let proof = servingProof(payment)
    let refundIdempotencyKey = refundKey(payment.id)
    defer async {
      if state.stage != .completed {
        do {
          let _ = try await billing.refund(take payment, idempotencyKey: refundIdempotencyKey)
        } catch error {
          Trace.current.recordCleanupError(error)
        }
      }
    }

    let receipt = try await diningRoom.serve(take dish, payment: proof)
    try state.advance(to: .completed)
    state.receipt = .some(receipt)
    orders[receipt.orderId] = state
    completedOrders += 1
    return receipt
  }

  async fn status(orderId: OrderId): ServiceStage throws RestaurantError {
    guard let ref state = orders[orderId] else throw .domain(.unknownOrder(orderId))
    return state.stage
  }

  mut async fn cancel(orderId: OrderId): CancelledStage throws RestaurantError {
    guard let inout state = orders[orderId] else throw .domain(.unknownOrder(orderId))
    try state.advance(to: .cancelled)
    return .cancelled
  }
}
