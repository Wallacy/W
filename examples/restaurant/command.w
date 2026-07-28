// Streaming command input for the Last Light restaurant.

import std.text
import { Course, Guest, GuestCount, GuestId, GuestName, Order, OrderId } from restaurant.domain

export type CommandLine = String<(value.bytes.count <= 65_536)>

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
}

export enum Command {
  place(Order)
  status(OrderId)
  cancel(OrderId)
  shutdown
}

fn decodeCourse(value: ref StringView): Course throws CommandError {
  return switch value {
    case "broth": .nebulaBroth
    case "souffle": .photonSouffle
    case "salad": .quietSalad
    case "cake": .horizonCake
    case _: throw .invalidCourse(value.toString())
  }
}

fn decodeOrder(fields: ref Array<StringView>, span: SourceSpan): Order throws CommandError {
  guard fields.count >= 5 else throw .missingField(name: "order", span: span)

  let orderId = try OrderId.parse(fields[1]).mapError((_) => .invalidNumber(name: "order-id", span: span))
  let guestId = try GuestId.parse(fields[2]).mapError((_) => .invalidNumber(name: "guest-id", span: span))
  let guests = try GuestCount.parse(fields[3]).mapError((_) => .invalidNumber(name: "guests", span: span))
  let course = try decodeCourse(fields[4])
  var notes: String? = .none

  if fields.count > 5 {
    notes = .some(String.join(fields.slice(from: 5), separator: " "))
  }

  return Order(
    id: orderId,
    guest: Guest(id: guestId, name: try GuestName("Traveller ${guestId}")),
    guests: guests,
    course: course,
    notes: notes,
  )
}

fn decodeOrderId(fields: ref Array<StringView>, span: SourceSpan): OrderId throws CommandError {
  guard fields.count >= 2 else throw .missingField(name: "order-id", span: span)
  return try OrderId.parse(fields[1]).mapError((_) => .invalidNumber(name: "order-id", span: span))
}

export fn decodeCommand(source: ref String): Command throws CommandError {
  let line = try CommandLine(source)
  let fields = line.views.split(where: (scalar) => scalar.isWhitespace)
  let span = SourceSpan(bytes: 0..<line.bytes.count, scalars: 0..<line.scalars.count)
  guard let verb = fields.first else throw .incompleteFrame(span)

  return switch verb {
    case "place": .place(try decodeOrder(fields, span: span))
    case "status": .status(try decodeOrderId(fields, span: span))
    case "cancel": .cancel(try decodeOrderId(fields, span: span))
    case "shutdown": .shutdown
    case _: throw .unknownVerb(verb.toString())
  }
}

export object CommandStream {
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
        commands.append(try decodeCommand(line))
        consumedBytes += line.bytes.count + 1
        consumedScalars += line.scalars.count + 1
      }
    }

    return commands
  }

  mut fn finish(): Array<Command> throws CommandError {
    if buffer.isEmpty {
      return []
    }

    let tail = buffer.takeAll()
    return [try decodeCommand(tail)]
  }

  deinit {
    buffer.clear()
  }
}

test "chunk boundaries do not change commands" for decodeCommand {
  let source = "place 42 7 3 cake please omit causality"
  let expected = try decodeCommand(source)
  var stream = CommandStream()
  let first = try stream.push("place 42 7 ")
  let second = try stream.push("3 cake please omit causality\n")

  expect first.isEmpty
  expect second == [expected]
}
