// Shared presentation model for terminal and HTTP hosts.

import {
  CancelledStage,
  OrderId,
  Receipt,
  ServiceStage,
  currencyCode,
} from domain
import { MenuItem } from billing
import { OrderSummary, RestaurantSnapshot } from restaurant
import { SimulationReport, writeSimulation } from simulation

export enum RenderMode {
  plain
  ansi
}

export enum AppResponse {
  help
  menu(Array<MenuItem>)
  placed(Receipt)
  status(OrderId, ServiceStage)
  cancelled(OrderId, CancelledStage)
  dashboard(RestaurantSnapshot)
  simulation(SimulationReport)
  shuttingDown
}

export const fn requestsShutdown(response: ref AppResponse): Bool {
  return switch response {
    case .shuttingDown: true
    case _: false
  }
}

fn serviceStageName(stage: ServiceStage): String {
  return switch stage {
    case .accepted: "accepted"
    case .reserving: "reserving"
    case .preparing: "preparing"
    case .serving: "serving"
    case .completed: "completed"
    case .cancelled: "cancelled"
  }
}

fn writeHeading(title: view String, mode: RenderMode, to output: inout String) {
  if mode == .ansi {
    output.append("\u{1B}[1;34m")
  }

  output.append(title)

  if mode == .ansi {
    output.append("\u{1B}[0m")
  }

  output.append("\n")
}

fn writeHelp(to output: inout String) {
  output.append("Commands\n")
  output.append("  help\n")
  output.append("  menu\n")
  output.append("  place <order-id> <guest-id> <guests> <broth|souffle|salad|cake> [notes]\n")
  output.append("  status <order-id>\n")
  output.append("  cancel <order-id>\n")
  output.append("  dashboard\n")
  output.append("  simulate <quiet|rush|timeline>\n")
  output.append("  shutdown\n")
}

fn writeMenu(items: ref Array<MenuItem>, to output: inout String) {
  for ref item in items {
    output.append("  ${item.label} — ${item.price.minorUnits} ${currencyCode(item.price.currency)} minor units\n")
  }
}

fn writeOrderSummary(order: ref OrderSummary, to output: inout String) {
  output.append("  #${order.orderId} ${serviceStageName(order.stage)}")

  if let total = order.total {
    output.append(" — ${total.minorUnits} ${currencyCode(total.currency)} minor units")
  }

  output.append("\n")
}

fn writeDashboard(snapshot: ref RestaurantSnapshot, to output: inout String) {
  output.append("Active: ${snapshot.activeOrders} | completed: ${snapshot.completedOrders}\n")

  if snapshot.orders.isEmpty {
    output.append("  No orders are observable.\n")
    return
  }

  for ref order in snapshot.orders {
    writeOrderSummary(order, to: output)
  }
}

export fn renderResponse(response: take AppResponse, mode: RenderMode): String {
  var output = String()

  if mode == .ansi {
    output.append("\u{1B}[2J\u{1B}[H")
  }

  switch response {
    case .help:
      writeHeading("Last Light Restaurant", mode: mode, to: output)
      writeHelp(to: output)
    case .menu(let items):
      writeHeading("Observable menu", mode: mode, to: output)
      writeMenu(items, to: output)
    case .placed(let receipt):
      writeHeading("Order accepted", mode: mode, to: output)
      output.append("  #${receipt.orderId} — ${receipt.total.minorUnits}")
      output.append(" ${currencyCode(receipt.total.currency)} minor units\n")
    case .status(let orderId, let stage):
      writeHeading("Order status", mode: mode, to: output)
      output.append("  #${orderId} — ${serviceStageName(stage)}\n")
    case .cancelled(let orderId, let stage):
      writeHeading("Order cancelled", mode: mode, to: output)
      output.append("  #${orderId} — ${serviceStageName(stage)}\n")
    case .dashboard(let snapshot):
      writeHeading("Service dashboard", mode: mode, to: output)
      writeDashboard(snapshot, to: output)
    case .simulation(let report):
      writeHeading("Deterministic shift", mode: mode, to: output)
      writeSimulation(report, to: output)
    case .shuttingDown:
      writeHeading("Service shutdown requested", mode: mode, to: output)
  }

  return output
}

test "plain and ANSI modes preserve the same semantic response" for renderResponse {
  let plain = renderResponse(.status(42, .preparing), mode: .plain)
  let ansi = renderResponse(.status(42, .preparing), mode: .ansi)

  expect plain.contains("#42")
  expect plain.contains("preparing")
  expect ansi.contains("#42")
  expect ansi.contains("\u{1B}[2J")
}
