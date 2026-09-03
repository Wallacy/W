// Integrated service route for the Turno do Horizonte Violeta.

import {
  CancelledStage,
  Dish,
  DomainError,
  Money,
  Order,
  OrderId,
  Receipt,
  ServiceStage,
  canMove,
} from domain
import {
  BillingApi,
  BillingError,
  MenuItem,
  billing,
  loadPriceTable,
  menuItems,
  paymentKey,
  quote,
  refundKey,
  servingProof,
} from billing
import { DiningRoomApi, DiningRoomError, diningRoom } from dining
import { AromaProbeApi, ProbeError, aromaProbe } from hardware
import {
  KitchenError,
  OvenApi,
  OvenError,
  PantryApi,
  PantryError,
  expectedEnergy,
  mix,
} from kitchen
import service {
  OvenApi as ovens,
  PantryApi as pantry,
} from kitchen
import { OracleApi, OracleError, oracle, planningRequest } from oracle

enum RestaurantError: Error {
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

struct OrderSummary {
  orderId: OrderId
  stage: ServiceStage
  total: Money?
}

struct RestaurantSnapshot {
  orders: Array<OrderSummary>
  activeOrders: u32
  completedOrders: u64
}

protocol RestaurantApi {
  async fn place(order: take Order): Receipt throws RestaurantError
  async fn status(orderId: OrderId): ServiceStage throws RestaurantError
  async fn cancel(orderId: OrderId): CancelledStage throws RestaurantError
  async fn menu(): Array<MenuItem> throws RestaurantError
  async fn snapshot(): RestaurantSnapshot
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
  pantry: ref ServiceRef<PantryApi>,
  ovens: ref ServiceRef<OvenApi>,
  oracle: ref ServiceRef<OracleApi>,
  probe: ref ServiceRef<AromaProbeApi>,
): Dish throws RestaurantError {
  let planning = planningRequest(order: order)
  let Order(guests, course, ...) = take order
  let stock = async pantry.reserve(course, guests: guests)
  let telemetry = async ovens.telemetry()
  let aromaSample = async probe.sample()
  let schedule = async oracle.plan(take planning)

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

  let projectedEnergy = expectedEnergy(telemetry: telemetry, during: schedule.duration)
  guard projectedEnergy <= schedule.energyBudget else {
    throw .kitchen(.energyBudgetExceeded(found: projectedEnergy, limit: schedule.energyBudget))
  }

  let mixture = spawn<.compute> mix(ingredients: stock.ingredients, recipe: schedule.recipe)

  let (lease, ready) = try await pipeline {
    let lease = ovens.acquire(schedule.recipe.target, duration: schedule.duration)
    let ready = lease.preheat()
    commit (lease, ready)
  }

  defer async {
    do {
      try await lease.close()
    } catch error {
      Trace.current.recordCleanupError(error)
    }
  }

  let mixture = try await mixture
  return try await lease.bake(take mixture, readiness: take ready)
}

service lastLight: RestaurantApi {
  var orders: Map<OrderId, OrderState> = Map()
  var Lazy priceTable = loadPriceTable()
  var completedOrders: u64 = 0

  mut async fn place(order: take Order): Receipt throws RestaurantError {
    let orderId = order.id
    var state = OrderState(stage: .accepted, receipt: .none)
    try state.advance(to: .reserving)
    orders[orderId] = state

    try state.advance(to: .preparing)
    orders[orderId] = state

    let dish = try await prepareDish(
      order: take order,
      pantry: pantry,
      ovens: ovens,
      oracle: oracle,
      probe: aromaProbe,
    )

    try state.advance(to: .serving)
    orders[orderId] = state

    let amount = try quote(policy: priceTable, course: dish.course)
    let payment = try await billing.capture(amount, idempotencyKey: paymentKey(orderId: orderId))
    let proof = servingProof(payment: payment)
    let refundIdempotencyKey = refundKey(paymentId: payment.id)
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
    guard let mut ref state = orders[orderId] else throw .domain(.unknownOrder(orderId))
    try state.advance(to: .cancelled)
    return .cancelled
  }

  async fn menu(): Array<MenuItem> throws RestaurantError {
    return try menuItems(table: priceTable)
  }

  async fn snapshot(): RestaurantSnapshot {
    var summaries: Array<OrderSummary> = []
    var activeOrders = 0_u32

    for entry in orders {
      let total: Money? = switch entry.value.receipt {
        case .some(let receipt): .some(receipt.total)
        case .none: .none
      }

      summaries.append(OrderSummary(
        orderId: entry.key,
        stage: entry.value.stage,
        total: total,
      ))

      if !(entry.value.stage in (.completed, .cancelled)) {
        activeOrders += 1
      }
    }

    summaries.sort(by: (left, right) => left.orderId.compare(right.orderId))
    return RestaurantSnapshot(
      orders: take summaries,
      activeOrders: activeOrders,
      completedOrders: completedOrders,
    )
  }
}

export {
  OrderSummary,
  RestaurantApi,
  RestaurantError,
  RestaurantSnapshot,
  lastLight,
  prepareDish,
}
