// Integrated service route for the Turno do Horizonte Violeta.

import {
  Dish,
  DomainError,
  Money,
  Order,
  OrderId,
  Receipt,
  ServiceStage,
  canMove,
} from restaurant.domain
import { Forecast, OracleError, forecast } from restaurant.oracle
import { AromaProbe, ProbeError } from restaurant.hardware
import {
  Duration,
  Energy,
  Power,
  Temperature,
  applauseThreshold,
  energy,
  serviceTemperature,
} from restaurant.units

export enum RestaurantError: Error {
  domain(DomainError)
  oracle(OracleError)
  probe(ProbeError)
  pantry(PantryError)
  oven(OvenError)
  billing(BillingError)
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

fn estimateBakeEnergy(power: Power, duration: Duration): Energy {
  return energy(power, during: duration)
}

async fn prepareDish(
  order: take Order,
  pantry: ServiceRef<PantryApi>,
  ovens: ServiceRef<OvenApi>,
  oracle: ServiceRef<OracleApi>,
): Dish throws RestaurantError {
  async on .network let stock = pantry.reserve(order.course, guests: order.guests)
  async on .network let telemetry = ovens.telemetry()
  spawn on .compute let schedule = oracle.plan(order)

  let (stock, telemetry, schedule) = try await (stock, telemetry, schedule)
  defer { stock.release() }

  let target = serviceTemperature
  let lease = try await ovens.acquire(target, schedule: schedule)
  defer async {
    await lease.close()
  }

  async let preheat = lease.preheat()
  spawn on .compute let mixture = mix(stock.ingredients, recipe: schedule.recipe)

  let ready = try await preheat
  let mixture = await mixture
  return try await lease.bake(mixture, until: ready.deadline)
}

export service LastLightRestaurant as RestaurantApi {
  let pantry: ServiceRef<PantryApi>
  let ovens: ServiceRef<OvenApi>
  let oracle: ServiceRef<OracleApi>
  let billing: ServiceRef<BillingApi>
  let diningRoom: ServiceRef<DiningRoomApi>
  var orders: Map<OrderId, OrderState>
  var Lazy priceTable = loadPriceTable()
  var atomic completedOrders: u64 = 0

  mut async fn place(order: take Order): Receipt throws RestaurantError {
    var state = OrderState(stage: .accepted, receipt: .none)
    try move(inout state, to: .reserving)
    orders[order.id] = state

    let dish = try await prepareDish(
      take order,
      pantry: pantry,
      ovens: ovens,
      oracle: oracle,
    )

    try move(inout state, to: .serving)
    let payment = try await billing.capture(priceTable.price(for: dish.course))
    defer async {
      if state.stage != .completed {
        await billing.refund(payment, idempotencyKey: payment.id)
      }
    }

    let receipt = try await diningRoom.serve(dish, payment: payment)
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
