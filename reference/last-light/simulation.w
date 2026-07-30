// Deterministic shift simulation for the Last Light restaurant.

import std.si
import {
  Course,
  Currency,
  DomainError,
  Guest,
  GuestCount,
  GuestId,
  GuestName,
  Money,
  Order,
  OrderId,
  SimulationProfile,
  add,
  courseLabel,
  currencyCode,
} from restaurant.domain
import { BillingError, PricingPolicy, loadPriceTable, quote } from restaurant.billing
import { DutyCycle, expectedEnergy } from restaurant.kitchen
import { Energy, PhysicalDuration, Power } from restaurant.units

export type SimulationTick = u16
export type SimulationTicks = u16<(1...1_000)>
export type SimulationCooks = u16<(1...256)>
export type SimulationTables = u16<(1...4_096)>

export enum SimulationStage {
  scheduled
  waiting
  cooking
  ready
  seated
  completed
  departed
}

export struct SimulationEvent {
  tick: SimulationTick
  orderId: OrderId
  stage: SimulationStage
}

export struct SimulationOrderSummary {
  orderId: OrderId
  guestName: GuestName
  course: Course
  timeline: u32
  stage: SimulationStage
}

export struct SimulationReport {
  profile: SimulationProfile
  maximumTicks: SimulationTicks
  cooks: SimulationCooks
  tables: SimulationTables
  tickDuration: PhysicalDuration
  ticksRun: SimulationTick
  completed: usize
  departed: usize
  unfinished: usize
  queueHighWater: usize
  energyUsed: Energy
  revenue: Money
  orders: Array<SimulationOrderSummary>
  events: Array<SimulationEvent>
}

export enum SimulationError: Error {
  billing(BillingError)
  domain(DomainError)
}

struct SimulationConfig {
  maximumTicks: SimulationTicks
  cooks: SimulationCooks
  tables: SimulationTables
  ovenPower: Power
  ovenDuty: DutyCycle
  tickDuration: PhysicalDuration
}

struct SimulatedOrder {
  order: Order
  arrivalTick: SimulationTick
  preparationTicks: SimulationTicks
  patienceTicks: SimulationTicks
  var preparationRemaining: SimulationTick
  var waitedTicks: SimulationTick
  var stage: SimulationStage
}

struct Scenario {
  config: SimulationConfig
  orders: Array<SimulatedOrder>
}

fn simulatedOrder(
  orderId: OrderId,
  guestId: GuestId,
  name: GuestName,
  guests: GuestCount,
  course: Course,
  arrivalTick: SimulationTick,
  preparationTicks: SimulationTicks,
  patienceTicks: SimulationTicks,
  timeline: u32,
): SimulatedOrder {
  return SimulatedOrder(
    order: Order(
      id: orderId,
      guest: Guest(id: guestId, name: name),
      guests: guests,
      course: course,
      notes: .some("Simulation profile ${timeline}"),
      timeline: timeline,
    ),
    arrivalTick: arrivalTick,
    preparationTicks: preparationTicks,
    patienceTicks: patienceTicks,
    preparationRemaining: preparationTicks,
    waitedTicks: 0,
    stage: .scheduled,
  )
}

fn scenario(profile: SimulationProfile): Scenario {
  return switch profile {
    case .quietOrbit:
      Scenario(
        config: SimulationConfig(
          maximumTicks: 24,
          cooks: 2,
          tables: 3,
          ovenPower: 12_000<si.W>,
          ovenDuty: 0.60,
          tickDuration: 30<si.s>,
        ),
        orders: [
          simulatedOrder(101, guestId: 501, name: "Ada Quasar", guests: 2, course: .quietSalad,
            arrivalTick: 0, preparationTicks: 2, patienceTicks: 8, timeline: 0),
          simulatedOrder(102, guestId: 502, name: "Capitão Ontem", guests: 4, course: .horizonCake,
            arrivalTick: 1, preparationTicks: 4, patienceTicks: 10, timeline: 0),
          simulatedOrder(103, guestId: 503, name: "Comitê de Uma Pessoa Só", guests: 1, course: .nebulaBroth,
            arrivalTick: 2, preparationTicks: 2, patienceTicks: 6, timeline: 0),
        ],
      )
    case .photonRush:
      Scenario(
        config: SimulationConfig(
          maximumTicks: 32,
          cooks: 3,
          tables: 4,
          ovenPower: 18_000<si.W>,
          ovenDuty: 0.82,
          tickDuration: 20<si.s>,
        ),
        orders: [
          simulatedOrder(201, guestId: 601, name: "Dra. Lira Pós-Agora", guests: 3, course: .photonSouffle,
            arrivalTick: 0, preparationTicks: 5, patienceTicks: 9, timeline: 0),
          simulatedOrder(202, guestId: 602, name: "Milo Sem Pressa", guests: 2, course: .quietSalad,
            arrivalTick: 0, preparationTicks: 2, patienceTicks: 5, timeline: 0),
          simulatedOrder(203, guestId: 603, name: "Fiscal do Último Minuto", guests: 8, course: .horizonCake,
            arrivalTick: 1, preparationTicks: 6, patienceTicks: 8, timeline: 0),
          simulatedOrder(204, guestId: 604, name: "Orquestra de Um Fóton", guests: 12, course: .nebulaBroth,
            arrivalTick: 1, preparationTicks: 3, patienceTicks: 7, timeline: 0),
          simulatedOrder(205, guestId: 605, name: "Turista do Setor Improvável", guests: 4, course: .photonSouffle,
            arrivalTick: 2, preparationTicks: 5, patienceTicks: 6, timeline: 0),
          simulatedOrder(206, guestId: 606, name: "Auditora da Causalidade", guests: 1, course: .quietSalad,
            arrivalTick: 3, preparationTicks: 2, patienceTicks: 4, timeline: 0),
        ],
      )
    case .timelineCollision:
      Scenario(
        config: SimulationConfig(
          maximumTicks: 20,
          cooks: 1,
          tables: 1,
          ovenPower: 9_000<si.W>,
          ovenDuty: 0.95,
          tickDuration: 45<si.s>,
        ),
        orders: [
          simulatedOrder(301, guestId: 701, name: "Visitante que Já Pagou Amanhã", guests: 2,
            course: .horizonCake, arrivalTick: 0, preparationTicks: 7, patienceTicks: 9, timeline: 7),
          simulatedOrder(302, guestId: 702, name: "Visitante que Ainda Não Chegou", guests: 2,
            course: .photonSouffle, arrivalTick: 0, preparationTicks: 6, patienceTicks: 3, timeline: 12),
          simulatedOrder(303, guestId: 703, name: "Visitante Duplicado Legalmente", guests: 2,
            course: .nebulaBroth, arrivalTick: 0, preparationTicks: 4, patienceTicks: 2, timeline: 7),
          simulatedOrder(304, guestId: 704, name: "Advogada do Paradoxo", guests: 1,
            course: .quietSalad, arrivalTick: 1, preparationTicks: 2, patienceTicks: 2, timeline: 99),
        ],
      )
  }
}

fn countStage(orders: ref Array<SimulatedOrder>, stage: SimulationStage): usize {
  var count = 0_usize

  for ref order in orders {
    if order.stage == stage { count += 1 }
  }

  return count
}

fn isTerminal(orders: ref Array<SimulatedOrder>): Bool {
  for ref order in orders {
    if !(order.stage in (.completed, .departed)) { return false }
  }

  return true
}

fn record(
  events: inout Array<SimulationEvent>,
  tick: SimulationTick,
  orderId: OrderId,
  stage: SimulationStage,
) {
  events.append(SimulationEvent(tick: tick, orderId: orderId, stage: stage))
}

export fn simulateShift(profile: SimulationProfile): SimulationReport throws SimulationError {
  let selectedScenario = scenario(profile)
  let SimulationConfig(maximumTicks, cooks, tables, ovenPower, ovenDuty, tickDuration) =
    selectedScenario.config
  var orders = take selectedScenario.orders
  let priceTable = loadPriceTable()
  let pricing: any PricingPolicy = priceTable
  let energyPerCookingTick = expectedEnergy(ovenPower, duty: ovenDuty, during: tickDuration)
  var events: Array<SimulationEvent> = []
  var revenue = Money.zeroCredits
  var energyUsed: Energy = 0<si.J>
  var queueHighWater = 0_usize
  var tick: SimulationTick = 0

  while tick < maximumTicks && !isTerminal(orders) {
    for inout candidate in orders {
      switch candidate.stage {
        case .scheduled if candidate.arrivalTick <= tick:
          candidate.stage = .waiting
          record(events, tick: tick, orderId: candidate.order.id, stage: .waiting)
        case .cooking:
          energyUsed += energyPerCookingTick

          if candidate.preparationRemaining > 1 {
            candidate.preparationRemaining -= 1
          } else {
            candidate.preparationRemaining = 0
            candidate.stage = .ready
            record(events, tick: tick, orderId: candidate.order.id, stage: .ready)
          }
        case .seated:
          let price = try quote(pricing, course: candidate.order.course)
          revenue = try add(revenue, to: price)
          candidate.stage = .completed
          record(events, tick: tick, orderId: candidate.order.id, stage: .completed)
        case _:
      }
    }

    let seated = countStage(orders, stage: .seated)
    var freeTables: usize = tables
    freeTables -= seated

    for inout candidate in orders {
      if candidate.stage == .ready && freeTables > 0 {
        candidate.stage = .seated
        freeTables -= 1
        record(events, tick: tick, orderId: candidate.order.id, stage: .seated)
      }
    }

    let cooking = countStage(orders, stage: .cooking)
    var freeCooks: usize = cooks
    freeCooks -= cooking

    for inout candidate in orders {
      if candidate.stage == .waiting && freeCooks > 0 {
        candidate.stage = .cooking
        freeCooks -= 1
        record(events, tick: tick, orderId: candidate.order.id, stage: .cooking)
      }
    }

    for inout candidate in orders {
      if candidate.stage == .waiting {
        candidate.waitedTicks += 1

        if candidate.waitedTicks >= candidate.patienceTicks {
          candidate.stage = .departed
          record(events, tick: tick, orderId: candidate.order.id, stage: .departed)
        }
      }
    }

    queueHighWater = max(queueHighWater, countStage(orders, stage: .waiting))
    tick += 1
  }

  var summaries: Array<SimulationOrderSummary> = []

  for ref candidate in orders {
    summaries.append(SimulationOrderSummary(
      orderId: candidate.order.id,
      guestName: copy candidate.order.guest.name,
      course: candidate.order.course,
      timeline: candidate.order.timeline,
      stage: candidate.stage,
    ))
  }

  let completed = countStage(orders, stage: .completed)
  let departed = countStage(orders, stage: .departed)

  return SimulationReport(
    profile: profile,
    maximumTicks: maximumTicks,
    cooks: cooks,
    tables: tables,
    tickDuration: tickDuration,
    ticksRun: tick,
    completed: completed,
    departed: departed,
    unfinished: orders.count - completed - departed,
    queueHighWater: queueHighWater,
    energyUsed: energyUsed,
    revenue: revenue,
    orders: take summaries,
    events: take events,
  )
}

export fn simulationProfileName(profile: SimulationProfile): String {
  return switch profile {
    case .quietOrbit: "quiet orbit"
    case .photonRush: "photon rush"
    case .timelineCollision: "timeline collision"
  }
}

export fn simulationStageName(stage: SimulationStage): String {
  return switch stage {
    case .scheduled: "scheduled"
    case .waiting: "waiting"
    case .cooking: "cooking"
    case .ready: "ready"
    case .seated: "seated"
    case .completed: "completed"
    case .departed: "departed"
  }
}

export fn writeSimulation(report: ref SimulationReport, to output: inout String) {
  output.append("Simulation: ${simulationProfileName(report.profile)}\n")
  output.append("Capacity: ${report.cooks} cooks | ${report.tables} tables")
  output.append(" | tick ${report.tickDuration.value(in: si.s)} s\n")
  output.append("Ticks: ${report.ticksRun} | completed: ${report.completed} | departed: ${report.departed}\n")
  output.append("Queue high-water: ${report.queueHighWater}\n")
  output.append("Energy: ${report.energyUsed.value(in: si.J)} J")
  output.append(" | revenue: ${report.revenue.minorUnits}")
  output.append(" ${currencyCode(report.revenue.currency)} minor units\n")
  output.append("\nFinal manifest\n")

  for ref order in report.orders {
    output.append("- #${order.orderId} ${order.guestName} | ${courseLabel(order.course)}")
    output.append(" | timeline ${order.timeline} | ${simulationStageName(order.stage)}\n")
  }

  output.append("\nEvent log\n")

  for ref event in report.events {
    output.append("  t=${event.tick} #${event.orderId} ${simulationStageName(event.stage)}\n")
  }
}

test "the quiet orbit completes without losing an order" for simulateShift {
  let report = try simulateShift(.quietOrbit)

  expect report.completed == 3
  expect report.departed == 0
  expect report.unfinished == 0
  expect report.ticksRun == 7
  expect report.revenue.minorUnits == 6_342
  expect report.revenue.currency == Currency.cr
  expect report.energyUsed > 0<si.J>
}

test "the timeline collision makes overload visible" for simulateShift {
  let report = try simulateShift(.timelineCollision)

  expect report.completed == 1
  expect report.departed == 3
  expect report.unfinished == 0
  expect report.ticksRun == 9
  expect report.queueHighWater == 2
  expect report.completed + report.departed + report.unfinished == report.orders.count
}

test "a simulation profile replays the same observable history" for simulateShift {
  let first = try simulateShift(.photonRush)
  let replay = try simulateShift(.photonRush)

  expect first.ticksRun == replay.ticksRun
  expect first.completed == replay.completed
  expect first.departed == replay.departed
  expect first.energyUsed == replay.energyUsed
  expect first.revenue.minorUnits == replay.revenue.minorUnits
  expect first.revenue.currency == replay.revenue.currency
  expect first.orders.count == replay.orders.count
  expect first.events.count == replay.events.count

  for index in 0..<first.events.count {
    let ref observed = first.events[index]
    let ref repeated = replay.events[index]

    expect observed.tick == repeated.tick
    expect observed.orderId == repeated.orderId
    expect observed.stage == repeated.stage
  }
}
