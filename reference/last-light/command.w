// Streaming command input for the Last Light restaurant.

import std.text
import {
  Course,
  Guest,
  GuestCount,
  GuestId,
  GuestName,
  Order,
  OrderId,
  SimulationProfile,
} from domain

export type CommandLine = String<(.bytes.count <= 65_536)>

export struct SourceSpan {
  bytes: Range<usize>
  scalars: Range<usize>
}

export enum CommandError: Error {
  frameTooLarge(found: usize, limit: usize)
  incompleteFrame(SourceSpan)
  unknownVerb(String)
  missingField(name: String, span: SourceSpan)
  invalidNumber(name: String, span: SourceSpan)
  invalidCourse(String)
  invalidSimulationProfile(String)
}

export enum Command {
  help
  menu
  place(Order)
  status(OrderId)
  cancel(OrderId)
  dashboard
  simulate(SimulationProfile)
  shutdown
}

fn decodeCourse(value: view String): Course throws CommandError {
  return switch value {
    case "broth": .nebulaBroth
    case "souffle": .photonSouffle
    case "salad": .quietSalad
    case "cake": .horizonCake
    case _: throw .invalidCourse(value.materialize())
  }
}

fn decodeOrder(fields: ref Array<view String>, span location: SourceSpan): Order throws CommandError {
  guard fields.count >= 5 else throw .missingField(name: "order", span: location)

  let orderIdCarrier = try u64.parse(fields[1]).mapError((_) => .invalidNumber(name: "order-id", span: location))
  let guestIdCarrier = try u64.parse(fields[2]).mapError((_) => .invalidNumber(name: "guest-id", span: location))
  let orderId = OrderId(orderIdCarrier)
  let guestId = GuestId(guestIdCarrier)
  let guests = try GuestCount.parse(fields[3]).mapError((_) => .invalidNumber(name: "guests", span: location))
  let course = try decodeCourse(value: fields[4])
  var notes: String? = .none

  if fields.count > 5 {
    notes = .some(String.join(fields[5...], separator: " "))
  }

  return Order(
    id: orderId,
    guest: Guest(id: guestId, name: try GuestName("Traveller ${guestId}")),
    guests: guests,
    course: course,
    notes: notes,
  )
}

fn decodeOrderId(fields: ref Array<view String>, span location: SourceSpan): OrderId throws CommandError {
  guard fields.count >= 2 else throw .missingField(name: "order-id", span: location)
  let carrier = try u64.parse(fields[1]).mapError((_) => .invalidNumber(name: "order-id", span: location))
  return OrderId(carrier)
}

fn decodeSimulationProfile(
  fields: ref Array<view String>,
  span location: SourceSpan,
): SimulationProfile throws CommandError {
  guard fields.count >= 2 else throw .missingField(name: "simulation-profile", span: location)

  return switch fields[1] {
    case "quiet": .quietOrbit
    case "rush": .photonRush
    case "timeline": .timelineCollision
    case _: throw .invalidSimulationProfile(fields[1].materialize())
  }
}

export fn decodeCommand(source: ref String): Command throws CommandError {
  let line = try CommandLine(source)
  let fields: Array<view String> = line.scalars
    .split(where: (scalar) => scalar.isWhitespace)
    .collect()
  let span = SourceSpan(bytes: 0..<line.bytes.count, scalars: 0..<line.scalars.count)
  guard let verb = fields.first else throw .incompleteFrame(span)

  return switch verb {
    case "help": .help
    case "menu": .menu
    case "place": .place(try decodeOrder(fields: fields, span: span))
    case "status": .status(try decodeOrderId(fields: fields, span: span))
    case "cancel": .cancel(try decodeOrderId(fields: fields, span: span))
    case "dashboard": .dashboard
    case "simulate": .simulate(try decodeSimulationProfile(fields: fields, span: span))
    case "shutdown": .shutdown
    case _: throw .unknownVerb(verb.materialize())
  }
}

object CommandStream {
  var buffer = String()
  var consumedBytes: usize = 0
  var consumedScalars: usize = 0

  mut fn push(chunk: take String): Array<Command> throws CommandError {
    var commands: Array<Command> = []

    for fragment in chunk.frameFragments(separator: '\n') {
      let nextSize = try usize.checkedAdd(buffer.bytes.count, fragment.text.bytes.count)
      guard nextSize <= 65_536 else throw .frameTooLarge(found: nextSize, limit: 65_536)
      buffer.append(fragment.text)

      if fragment.terminatesFrame {
        let line = buffer.takeAll()
        commands.append(try decodeCommand(source: line))
        consumedBytes += line.bytes.count + 1
        consumedScalars += line.scalars.count + 1
      }
    }

    return commands
  }

  take fn finish(): Array<Command> throws CommandError {
    if buffer.isEmpty {
      return []
    }

    let tail = buffer.takeAll()
    return [try decodeCommand(source: tail)]
  }

  deinit {
    buffer.clear()
  }
}

test "chunk boundaries do not change commands" for decodeCommand {
  let source = "place 42 7 3 cake please omit causality"
  let expected = try decodeCommand(source: source)
  var cursor = CommandStream()
  let first = try cursor.push("place 42 7 ")
  let second = try cursor.push("3 cake please omit causality\n")
  let tail = try (take cursor).finish()

  expect first.isEmpty
  expect second == [expected]
  expect tail.isEmpty
}

test "simulation profiles are closed and typed" for decodeCommand {
  expect try decodeCommand(source: "simulate quiet") == .simulate(.quietOrbit)
  expect try decodeCommand(source: "simulate rush") == .simulate(.photonRush)
  expect try decodeCommand(source: "simulate timeline") == .simulate(.timelineCollision)
}
