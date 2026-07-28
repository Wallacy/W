// Integrated service route for the Turno do Horizonte Violeta.

import {
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
import { OracleApi, OracleError } from restaurant.oracle

export enum RestaurantError: Error {
  domain(DomainError)
  oracle(OracleError)
  probe(ProbeError)
  pantry(PantryError)
  oven(OvenError)
  kitchen(KitchenError)
  billing(BillingError)
  dining(DiningRoomError)
  overload
}

export protocol RestaurantApi {
  async fn place(order: take Order): Receipt throws RestaurantError
  async fn status(orderId: OrderId): ServiceStage throws RestaurantError
  async fn cancel(orderId: OrderId): ServiceStage throws RestaurantError
}

struct OrderState {
  stage: ServiceStage
  receipt: Receipt?
}

fn move(state: inout OrderState, to next: ServiceStage) throws DomainError {
  guard canMove(from: state.stage, to: next) else {
    throw .invalidTransition(from: state.stage, to: next)
  }

  state.stage = next
}

async fn prepareDish(
  order: take Order,
  pantry: ServiceRef<PantryApi>,
  ovens: ServiceRef<OvenApi>,
  oracle: ServiceRef<OracleApi>,
  probe: ServiceRef<AromaProbeApi>,
): Dish throws RestaurantError {
  async on .network let stock = pantry.reserve(order.course, guests: order.guests)
  async on .device let telemetry = ovens.telemetry()
  async on .device let aromaSample = probe.sample()
  spawn on .compute let schedule = oracle.plan(order)

  let (stock, telemetry, aromaSample, schedule) = try await (stock, telemetry, aromaSample, schedule)
  defer async {
    do {
      try await stock.release()
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

  let lease = try await ovens.acquire(schedule.recipe.target, schedule: schedule)
  defer async {
    await lease.close()
  }

  async let preheat = lease.preheat()
  spawn on .compute let mixture = mix(stock.ingredients, recipe: schedule.recipe)

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
    try move(inout state, to: .reserving)
    orders[orderId] = state

    try move(inout state, to: .preparing)
    orders[orderId] = state

    let dish = try await prepareDish(
      take order,
      pantry: pantry,
      ovens: ovens,
      oracle: oracle,
      probe: probe,
    )

    try move(inout state, to: .serving)
    orders[orderId] = state

    let amount = try quote(priceTable, course: dish.course)
    let payment = try await billing.capture(amount, idempotencyKey: paymentKey(orderId))
    defer async {
      if state.stage != .completed {
        do {
          let _ = try await billing.refund(payment, idempotencyKey: refundKey(payment.id))
        } catch error {
          Trace.current.recordCleanupError(error)
        }
      }
    }

    let receipt = try await diningRoom.serve(take dish, payment: payment)
    try move(inout state, to: .completed)
    state.receipt = .some(receipt)
    orders[receipt.orderId] = state
    completedOrders += 1
    return receipt
  }

  async fn status(orderId: OrderId): ServiceStage throws RestaurantError {
    guard let state = orders[orderId] else throw .domain(.unknownOrder(orderId))
    return state.stage
  }

  mut async fn cancel(orderId: OrderId): ServiceStage throws RestaurantError {
    guard var state = orders[orderId] else throw .domain(.unknownOrder(orderId))
    try move(inout state, to: .cancelled)
    orders[orderId] = state
    return state.stage
  }
}
