// Directional JSON documents for HTTP boundaries.
//
// Domain values do not conform to json.Codable.  Inbound documents use raw
// strings and base scalars, then validate into domain values.  Outbound
// documents borrow the domain response and encode synchronously.
// These vectors are compile/provider-gated until std.json@1 and std.http@1
// exist.  They do not claim provider execution.

import http from std
import json from std.json
import si from std
import {
  AppResponse,
} from presentation
import { Command } from command
import {
  Course,
  Guest,
  GuestCount,
  GuestId,
  GuestName,
  Money,
  Order,
  OrderId,
  Receipt,
  ServiceStage,
  SimulationProfile,
  currencyCode,
} from domain
import { MenuItem } from billing
import { OrderSummary, RestaurantSnapshot } from restaurant
import {
  SimulationEvent,
  SimulationOrderSummary,
  SimulationReport,
  SimulationStage,
} from simulation

export struct CommandGuestDocument: json.Decodable {
  id: String
  name: String
}

export struct CommandOrderDocument: json.Decodable {
  id: String
  guest: CommandGuestDocument
  guests: u16
  course: String
  notes: String?
  timeline: u32

  take fn command(): Order throws CommandDocumentError {
    let id = try canonicalOrderId(id, field: .orderId)
    let guestId = try canonicalGuestId(guest.id, field: .guestId)
    let guestName = try GuestName(guest.name.materialize())
      .mapError((_) => CommandDocumentError.invalidRefinement(.guestName))
    let guestCount = try GuestCount(guests)
      .mapError((_) => CommandDocumentError.invalidRange(.guests))
    let course = try courseFromToken(course)

    return Order(
      id: id,
      guest: Guest(id: guestId, name: guestName),
      guests: guestCount,
      course: course,
      notes: notes,
      timeline: timeline,
    )
  }
}

export struct CommandDocument: json.Decodable {
  kind: String
  order: CommandOrderDocument?
  orderId: String?
  profile: String?

  fn noPayload(): () throws CommandDocumentError {
    switch order {
      case .some: throw .unexpectedPayload(.order)
      case .none: ()
    }
    switch orderId {
      case .some: throw .unexpectedPayload(.orderId)
      case .none: ()
    }
    switch profile {
      case .some: throw .unexpectedPayload(.profile)
      case .none: ()
    }
  }

  export take fn command(): Command throws CommandDocumentError {
    return switch kind {
      case "help":
        try noPayload()
        .help
      case "menu":
        try noPayload()
        .menu
      case "dashboard":
        try noPayload()
        .dashboard
      case "shutdown":
        try noPayload()
        .shutdown
      case "status":
        guard let text = orderId else throw .missingPayload(.orderId)
        guard isMissing(order) else throw .unexpectedPayload(.order)
        guard isMissing(profile) else throw .unexpectedPayload(.profile)
        .status(try canonicalOrderId(text, field: .orderId))
      case "cancel":
        guard let text = orderId else throw .missingPayload(.orderId)
        guard isMissing(order) else throw .unexpectedPayload(.order)
        guard isMissing(profile) else throw .unexpectedPayload(.profile)
        .cancel(try canonicalOrderId(text, field: .orderId))
      case "simulate":
        guard let token = profile else throw .missingPayload(.profile)
        guard isMissing(order) else throw .unexpectedPayload(.order)
        guard isMissing(orderId) else throw .unexpectedPayload(.orderId)
        .simulate(try simulationProfileFromToken(token))
      case "place":
        guard let order = order else throw .missingPayload(.order)
        guard isMissing(orderId) else throw .unexpectedPayload(.orderId)
        guard isMissing(profile) else throw .unexpectedPayload(.profile)
        .place(try (take order).command())
      case _:
        throw .invalidKind(.kind)
    }
  }
}

fn isMissing<T>(value: T?): Bool {
  return switch value {
    case .none: true
    case .some: false
  }
}

export enum CommandDocumentField: Copy & Equatable {
  kind
  order
  orderId
  guestId
  guestName
  guests
  course
  notes
  timeline
  profile
}

export enum CommandDocumentError: Error {
  invalidKind(CommandDocumentField)
  missingPayload(CommandDocumentField)
  unexpectedPayload(CommandDocumentField)
  invalidToken(CommandDocumentField)
  invalidDecimal(CommandDocumentField)
  nonCanonicalDecimal(CommandDocumentField)
  invalidRange(CommandDocumentField)
  invalidRefinement(CommandDocumentField)
}

fn canonicalOrderId(
  text: view String,
  field: CommandDocumentField,
): OrderId throws CommandDocumentError {
  let carrier = try u64.parse(text)
    .mapError((_) => CommandDocumentError.invalidDecimal(field))
  guard text == carrier.display() else throw .nonCanonicalDecimal(field)
  return OrderId(carrier)
}

fn canonicalGuestId(
  text: view String,
  field: CommandDocumentField,
): GuestId throws CommandDocumentError {
  let carrier = try u64.parse(text)
    .mapError((_) => CommandDocumentError.invalidDecimal(field))
  guard text == carrier.display() else throw .nonCanonicalDecimal(field)
  return GuestId(carrier)
}

fn courseFromToken(value: String): Course throws CommandDocumentError {
  return switch value {
    case "nebula-broth": .nebulaBroth
    case "photon-souffle": .photonSouffle
    case "quiet-salad": .quietSalad
    case "horizon-cake": .horizonCake
    case _: throw .invalidToken(.course)
  }
}

fn simulationProfileFromToken(value: String): SimulationProfile throws CommandDocumentError {
  return switch value {
    case "quiet-orbit": .quietOrbit
    case "photon-rush": .photonRush
    case "timeline-collision": .timelineCollision
    case _: throw .invalidToken(.profile)
  }
}

fn courseToken(course: Course): String {
  return switch course {
    case .nebulaBroth: "nebula-broth"
    case .photonSouffle: "photon-souffle"
    case .quietSalad: "quiet-salad"
    case .horizonCake: "horizon-cake"
  }
}

fn profileToken(profile: SimulationProfile): String {
  return switch profile {
    case .quietOrbit: "quiet-orbit"
    case .photonRush: "photon-rush"
    case .timelineCollision: "timeline-collision"
  }
}

fn stageToken(stage: ServiceStage): String {
  return switch stage {
    case .accepted: "accepted"
    case .reserving: "reserving"
    case .preparing: "preparing"
    case .serving: "serving"
    case .completed: "completed"
    case .cancelled: "cancelled"
  }
}

fn simulationStageToken(stage: SimulationStage): String {
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

fn decimal(value: u64): String { value.display() }
fn decimalOrderId(value: OrderId): String { u64(value).display() }
fn decimalSigned(value: i128): String { value.display() }

struct MoneyDocument: json.Encodable {
  money: ref Money

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let minorUnits = decimalSigned(money.minorUnits)
    let currency = currencyCode(money.currency)
    try writer.withObject((object) => {
      try object.field("minorUnits", value: ref minorUnits)
      try object.field("currency", value: ref currency)
    })
  }
}

struct MenuItemDocument: json.Encodable {
  item: ref MenuItem

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let course = courseToken(item.course)
    let price = MoneyDocument(money: ref item.price)
    try writer.withObject((object) => {
      try object.field("course", value: ref course)
      try object.field("label", value: ref item.label)
      try object.field("price", value: ref price)
    })
  }
}

struct MenuItemsDocument: json.Encodable {
  items: ref Array<MenuItem>

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    try writer.withArray((array) => {
      for ref item in items {
        let document = MenuItemDocument(item: ref item)
        try array.element(value: ref document)
      }
    })
  }
}

struct ReceiptDocument: json.Encodable {
  receipt: ref Receipt

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let orderId = decimalOrderId(receipt.orderId)
    let total = MoneyDocument(money: ref receipt.total)
    try writer.withObject((object) => {
      try object.field("orderId", value: ref orderId)
      try object.field("total", value: ref total)
    })
  }
}

struct OrderSummaryDocument: json.Encodable {
  summary: ref OrderSummary

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let orderId = decimalOrderId(summary.orderId)
    let stage = stageToken(summary.stage)
    try writer.withObject((object) => {
      try object.field("orderId", value: ref orderId)
      try object.field("stage", value: ref stage)
      switch summary.total {
        case .some(let total):
          let encoded = MoneyDocument(money: ref total)
          try object.field("total", value: ref encoded)
        case .none:
          var null = json.Value.null
          try object.field("total", value: ref null)
      }
    })
  }
}

struct OrderSummariesDocument: json.Encodable {
  summaries: ref Array<OrderSummary>

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    try writer.withArray((array) => {
      for ref summary in summaries {
        let document = OrderSummaryDocument(summary: ref summary)
        try array.element(value: ref document)
      }
    })
  }
}

struct SnapshotDocument: json.Encodable {
  snapshot: ref RestaurantSnapshot

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let completedOrders = decimal(snapshot.completedOrders)
    let orders = OrderSummariesDocument(summaries: ref snapshot.orders)
    try writer.withObject((object) => {
      try object.field("orders", value: ref orders)
      try object.field("activeOrders", value: ref snapshot.activeOrders)
      try object.field("completedOrders", value: ref completedOrders)
    })
  }
}

struct SimulationOrderDocument: json.Encodable {
  order: ref SimulationOrderSummary

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let orderId = decimalOrderId(order.orderId)
    let course = courseToken(order.course)
    let stage = simulationStageToken(order.stage)
    try writer.withObject((object) => {
      try object.field("orderId", value: ref orderId)
      try object.field("guestName", value: ref order.guestName)
      try object.field("course", value: ref course)
      try object.field("timeline", value: ref order.timeline)
      try object.field("stage", value: ref stage)
    })
  }
}

struct SimulationEventDocument: json.Encodable {
  event: ref SimulationEvent

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let orderId = decimalOrderId(event.orderId)
    let stage = simulationStageToken(event.stage)
    try writer.withObject((object) => {
      try object.field("tick", value: ref event.tick)
      try object.field("orderId", value: ref orderId)
      try object.field("stage", value: ref stage)
    })
  }
}

struct SimulationOrdersDocument: json.Encodable {
  orders: ref Array<SimulationOrderSummary>

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    try writer.withArray((array) => {
      for ref order in orders {
        let document = SimulationOrderDocument(order: ref order)
        try array.element(value: ref document)
      }
    })
  }
}

struct SimulationEventsDocument: json.Encodable {
  events: ref Array<SimulationEvent>

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    try writer.withArray((array) => {
      for ref event in events {
        let document = SimulationEventDocument(event: ref event)
        try array.element(value: ref document)
      }
    })
  }
}

struct SecondsDocument: json.Encodable {
  value: f64

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let unit = "s"
    try writer.withObject((object) => {
      try object.field("value", value: ref value)
      try object.field("unit", value: ref unit)
    })
  }
}

struct JoulesDocument: json.Encodable {
  value: f64

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let unit = "J"
    try writer.withObject((object) => {
      try object.field("value", value: ref value)
      try object.field("unit", value: ref unit)
    })
  }
}

struct SimulationReportDocument: json.Encodable {
  report: ref SimulationReport

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let profile = profileToken(report.profile)
    let tickValue = report.tickDuration.value(in: si.s)
    let energyValue = report.energyUsed.value(in: si.J)
    let tickDuration = SecondsDocument(value: tickValue)
    let energyUsed = JoulesDocument(value: energyValue)
    let revenue = MoneyDocument(money: ref report.revenue)
    let orders = SimulationOrdersDocument(orders: ref report.orders)
    let events = SimulationEventsDocument(events: ref report.events)
    try writer.withObject((object) => {
      try object.field("profile", value: ref profile)
      try object.field("maximumTicks", value: ref report.maximumTicks)
      try object.field("cooks", value: ref report.cooks)
      try object.field("tables", value: ref report.tables)
      try object.field("tickDuration", value: ref tickDuration)
      try object.field("ticksRun", value: ref report.ticksRun)
      try object.field("completed", value: ref report.completed)
      try object.field("departed", value: ref report.departed)
      try object.field("unfinished", value: ref report.unfinished)
      try object.field("queueHighWater", value: ref report.queueHighWater)
      try object.field("energyUsed", value: ref energyUsed)
      try object.field("revenue", value: ref revenue)
      try object.field("orders", value: ref orders)
      try object.field("events", value: ref events)
    })
  }
}

// This document is borrowed.  Response.json calls encode synchronously and
// does not copy or move the AppResponse domain model.
export struct AppResponseDocument: json.Encodable {
  response: ref AppResponse

  export init(response: ref AppResponse) {
    self.response = response
  }

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    try writer.withObject((object) => {
      // `response` is a shared borrow.  Enum payload bindings below remain
      // borrowed; no case pattern consumes or copies the AppResponse model.
      switch response {
        case .help:
          let kind = "help"
          try object.field("kind", value: ref kind)
        case .shuttingDown:
          let kind = "shutting-down"
          try object.field("kind", value: ref kind)
        case .menu(let items):
          let kind = "menu"
          let encoded = MenuItemsDocument(items: ref items)
          try object.field("kind", value: ref kind)
          try object.field("items", value: ref encoded)
        case .placed(let receipt):
          let kind = "placed"
          let encoded = ReceiptDocument(receipt: ref receipt)
          try object.field("kind", value: ref kind)
          try object.field("receipt", value: ref encoded)
        case .status(let orderId, let stage):
          let kind = "status"
          let id = decimalOrderId(orderId)
          let token = stageToken(stage)
          try object.field("kind", value: ref kind)
          try object.field("orderId", value: ref id)
          try object.field("stage", value: ref token)
        case .cancelled(let orderId, let stage):
          let kind = "cancelled"
          let id = decimalOrderId(orderId)
          let token = stageToken(stage)
          try object.field("kind", value: ref kind)
          try object.field("orderId", value: ref id)
          try object.field("stage", value: ref token)
        case .dashboard(let snapshot):
          let kind = "dashboard"
          let encoded = SnapshotDocument(snapshot: ref snapshot)
          try object.field("kind", value: ref kind)
          try object.field("snapshot", value: ref encoded)
        case .simulation(let report):
          let kind = "simulation"
          let encoded = SimulationReportDocument(report: ref report)
          try object.field("kind", value: ref kind)
          try object.field("report", value: ref encoded)
      }
    })
  }
}

export enum ProblemCode: Copy & Equatable {
  malformedJson
  invalidCommand
  forbiddenShutdown
  invalidWifiDocument
}

export fn problemStatus(code: ProblemCode): http.StatusCode {
  return switch code {
    case .malformedJson: http.StatusCode.badRequest
    case .invalidCommand: http.StatusCode.unprocessableContent
    case .forbiddenShutdown: http.StatusCode.forbidden
    case .invalidWifiDocument: http.StatusCode.unprocessableContent
  }
}

fn problemTypeUri(code: ProblemCode): String {
  return switch code {
    case .malformedJson: "https://last-light.example/problems/malformed-json"
    case .invalidCommand: "https://last-light.example/problems/invalid-command"
    case .forbiddenShutdown: "https://last-light.example/problems/forbidden-shutdown"
    case .invalidWifiDocument: "https://last-light.example/problems/invalid-wifi-document"
  }
}

fn problemCodeToken(code: ProblemCode): String {
  return switch code {
    case .malformedJson: "malformed-json"
    case .invalidCommand: "invalid-command"
    case .forbiddenShutdown: "forbidden-shutdown"
    case .invalidWifiDocument: "invalid-wifi-document"
  }
}

fn problemTitle(code: ProblemCode): String {
  return switch code {
    case .malformedJson: "Malformed JSON"
    case .invalidCommand: "Invalid command document"
    case .forbiddenShutdown: "Shutdown is forbidden"
    case .invalidWifiDocument: "Invalid Wi-Fi document"
  }
}

fn problemDetail(code: ProblemCode): String {
  return switch code {
    case .malformedJson: "The request body is not a valid JSON document."
    case .invalidCommand: "The command document is not valid for this endpoint."
    case .forbiddenShutdown: "The caller cannot request shutdown."
    case .invalidWifiDocument: "The Wi-Fi document is not valid for this endpoint."
  }
}

export struct ProblemDocument: json.Encodable {
  code: ProblemCode

  export status: http.StatusCode {
    get => problemStatus(code)
  }

  fn encode(to writer: inout json.Writer) throws json.EncodeError {
    let problemType = problemTypeUri(code)
    let title = problemTitle(code)
    let status = self.status
    let token = problemCodeToken(code)
    let detail = problemDetail(code)
    try writer.withObject((object) => {
      try object.field("type", value: ref problemType)
      try object.field("title", value: ref title)
      try object.field("status", value: ref status)
      try object.field("code", value: ref token)
      try object.field("detail", value: ref detail)
    })
  }
}

export fn problem(code: ProblemCode): ProblemDocument {
  return ProblemDocument(code: code)
}

export fn problemResponse(
  code: ProblemCode,
  maximumBytes: usize<(1...)>,
): http.Response throws http.ResponseError {
  var headers = http.Headers()
  do {
    try headers.set("content-type", "application/problem+json")
  } catch error {
    throw .headers(error)
  }
  let document = ProblemDocument(code: code)
  let status = document.status
  return try http.Response.json(
    value: ref document,
    maximumBytes: maximumBytes,
    status: status,
    headers: take headers,
  )
}

test "command document uses tagged shapes and canonical decimal strings" {
  var source: Bytes = b"{\"kind\":\"place\",\"order\":{\"id\":\"42\",\"guest\":{\"id\":\"7\",\"name\":\"Arthur Dent\"},\"guests\":2,\"course\":\"horizon-cake\",\"notes\":null,\"timeline\":0}}"
  let document = try json.decode<CommandDocument>(ref source, limits: json.Limits(maximumBytes: 4<KiB>))
  let command = try (take document).command()

  switch command {
    case .place(let order):
      expect order.id == 42
      expect order.guest.id == 7
      expect order.course == .horizonCake
    case _:
      panic("place document decoded to another command")
  }
}

test "command document rejects noncanonical or unexpected payloads" {
  var noncanonical: Bytes = b"{\"kind\":\"status\",\"orderId\":\"+42\"}"
  let plus = try json.decode<CommandDocument>(ref noncanonical, limits: json.Limits(maximumBytes: 1<KiB>))
  do {
    let _ = try (take plus).command()
    panic("noncanonical order id was accepted")
  } catch .nonCanonicalDecimal(.orderId) {}

  var payload: Bytes = b"{\"kind\":\"help\",\"orderId\":\"42\"}"
  let unexpected = try json.decode<CommandDocument>(ref payload, limits: json.Limits(maximumBytes: 1<KiB>))
  do {
    let _ = try (take unexpected).command()
    panic("unexpected payload was accepted")
  } catch .unexpectedPayload(.orderId) {}
}

test "app response omits trace id and writes kind first" {
  let response = AppResponse.placed(Receipt(
    orderId: 42,
    total: Money(minorUnits: 4_242, currency: .cr),
    traceId: Trace.current.id,
  ))
  let document = AppResponseDocument(response: ref response)
  let bytes = try json.encode(ref document, limits: json.Limits(maximumBytes: 4<KiB>))
  expect bytes == b"{\"kind\":\"placed\",\"receipt\":{\"orderId\":\"42\",\"total\":{\"minorUnits\":\"4242\",\"currency\":\"CR\"}}}"
}

test "problem response keeps code, status, body, and media type aligned" {
  let code = ProblemCode.invalidCommand
  expect problemStatus(.malformedJson) == http.StatusCode.badRequest
  expect problemStatus(.forbiddenShutdown) == http.StatusCode.forbidden
  expect problemStatus(.invalidWifiDocument) == http.StatusCode.unprocessableContent
  let body = problem(code: code)
  expect body.status == http.StatusCode.unprocessableContent
  let bytes = try json.encode(ref body, limits: json.Limits(maximumBytes: 1<KiB>))
  expect bytes == b"{\"type\":\"https://last-light.example/problems/invalid-command\",\"title\":\"Invalid command document\",\"status\":422,\"code\":\"invalid-command\",\"detail\":\"The command document is not valid for this endpoint.\"}"

  let response = try problemResponse(code: code, maximumBytes: 1<KiB>)
  expect response.status == http.StatusCode.unprocessableContent
  expect try response.headers.get("content-type") == "application/problem+json"
}
