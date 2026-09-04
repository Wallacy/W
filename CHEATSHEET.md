# W language cheatsheet

> Design draft. Every example is a complete syntax unit: it declares a feature,
> uses it, and shows an observable result. Some units are design/source evidence
> until the corresponding compiler or runtime feature exists.

## Contents

- [Programs, entries, and execution context](#programs-entries-and-execution-context)
- [Modules, imports, exports, and reexports](#modules-imports-exports-and-reexports)
- [Documentation and tests](#documentation-and-tests)
- [Literals and interpolation](#literals-and-interpolation)
- [Collections and ranges](#collections-and-ranges)
- [Operators and pipe-forward](#operators-and-pipe-forward)
- [Numeric policies and bit primitives](#numeric-policies-and-bit-primitives)
- [Functions, labels, defaults, and rest](#functions-labels-defaults-and-rest)
- [Structs, objects, enums, and extensions](#structs-objects-enums-and-extensions)
- [Protocols, generics, refinements, and specialization](#protocols-generics-refinements-and-specialization)
- [Properties, behaviors, and facets](#properties-behaviors-and-facets)
- [Option, conversion, and type queries](#option-conversion-and-type-queries)
- [Ownership, borrows, and views](#ownership-borrows-and-views)
- [Callable values and captures](#callable-values-and-captures)
- [Control flow and patterns](#control-flow-and-patterns)
- [Errors and cleanup](#errors-and-cleanup)
- [Allocator scopes](#allocator-scopes)
- [Unsafe, addresses, and bit operations](#unsafe-addresses-and-bit-operations)
- [Async, spawn, sync, and await](#async-spawn-sync-and-await)
- [Tasks and cancellation](#tasks-and-cancellation)
- [Bounded task pipelines](#bounded-task-pipelines)
- [Service and transaction pipelines](#service-and-transaction-pipelines)
- [Streams and channels](#streams-and-channels)
- [Shared state, atomics, and locks](#shared-state-atomics-and-locks)
- [Units, matrices, tensors, and SIMD](#units-matrices-tensors-and-simd)
- [Foreign code and ABI](#foreign-code-and-abi)
- [Packages and workspaces](#packages-and-workspaces)

## Programs, entries, and execution context

Short body, anonymous `.default` descriptor:

<!-- w-example role=executable use=print observable=effect -->
```w
module hello

entry {
  print("Hello, world!")
}
```

Default function descriptor with an explicit host signature:

<!-- w-example role=logical-contract -->
```w
module commandLine

import {
  Arguments as ProcessArguments,
  Context as ProcessContext,
  ExitCode as ProcessExitCode,
} from std.process

async fn run(
  args: ProcessArguments,
  context: ProcessContext,
): ProcessExitCode {
  execution#checkCancellation()
  let started = execution.clock().now()
  await execution#yield()
  context.output.write("arguments=${args.count}, started=${started}")
  return .success
}

entry(run)
```

Named function descriptor:

<!-- w-example role=executable use=diagnose observable=effect -->
```w
module diagnostics

fn diagnose() { print("ready") }

entry Diagnostics(diagnose)
```

Named body descriptor:

<!-- w-example role=executable use=print observable=effect -->
```w
module embeddedDiagnostics

entry Diagnostics {
  print("ready")
}
```

## Modules, imports, exports, and reexports

Package import exposes its public modules without flattening them:

<!-- w-example role=logical-contract -->
```w
module packageImport

import std

fn argumentCount(args: process.Arguments): usize {
  io.print("count=${args.count}")
  return args.count
}
```

Direct and wildcard module imports flatten the same exports:

<!-- w-example role=logical-contract -->
```w
module flatImports

import std.process
// Equivalent spelling in another module: import * from std.process

fn succeeded(code: ExitCode): Bool { return code == .success }
```

A module binding keeps qualification; braces select symbols:

<!-- w-example role=logical-contract -->
```w
module selectedImports

import process from std
import networkUrl from std.url
import {
  Arguments as ProcessArguments,
  ExitCode,
} from std.process

fn inspect(args: ProcessArguments, url: networkUrl.URL): (ExitCode, String) {
  return (.success, "${args.count}:${url}")
}
```

Reexports preserve the same distinction:

<!-- w-example role=logical-contract -->
```w
module publicApi

export * from std.url
export { Arguments as ProcessArguments } from std.process

export fn apiName(): String { return "public-api" }
```

## Documentation and tests

<!-- w-example role=executable use=clamp observable=value -->
```w
/// Limits a value to an inclusive interval.
///
/// call: clamp(4, minimum: 0, maximum: 3)
/// result: 3
/// call: clamp(2, minimum: 0, maximum: 3)
/// result: 2
fn clamp(_ value: i32, minimum: i32, maximum: i32): i32 {
  return value.max(minimum).min(maximum)
}

test "clamp preserves an internal value" for clamp {
  expect clamp(2, minimum: 0, maximum: 3) == 2
}
```

## Literals and interpolation

<!-- w-example role=executable use=literalSummary observable=value -->
```w
fn literalSummary(_ seconds: u64): (String, String, String, String) {
  let integer: i32 = 1_000
  let hexadecimal = 0xff
  let ratio: f64 = 0.5e2
  let enabled: Bool = true
  let inferredText = 'W'
  let scalar: UnicodeScalar = 'λ'
  let byte: u8 = b'W'
  let bytes = b"W"
  let json = '{"value":${seconds},"unit":"s"}'
  let doubleQuoted = "${integer}:${hexadecimal}:${ratio}:${enabled}:${scalar}:${byte}"
  let builtins = "Kitchen ${true}/${false}; table: ${"open"}"
  let raw = #"C:\orders\${seconds}"#
  let rawSingle = #'C:\orders\${seconds}'#
  let multiline = """
    north ${seconds}
    south
    """
  let singleMultiline = '''
    east ${seconds}
    west
    '''
  let rawMultiline = #"""
    ${seconds}
    C:\orders
    """#
  expect inferredText == "W"
  expect bytes.count == 1
  expect builtins == "Kitchen true/false; table: open"
  return (
    json,
    doubleQuoted,
    raw + rawSingle,
    multiline + singleMultiline + rawMultiline,
  )
}

test "ordinary strings interpolate and raw strings do not" for literalSummary {
  let result = literalSummary(30)
  expect result.0 == #'{"value":30,"unit":"s"}'#
  expect result.2.contains(#'${seconds}'#)
  expect result.3.contains("north 30")
  expect result.3.contains(#'${seconds}'#)
}
```

## Collections and ranges

<!-- w-example role=executable use=collectionSummary observable=value -->
```w
fn collectionSummary(): (i32, i32, i32, usize, i32) {
  let positional = (1, "north")
  let singleton = (7,)
  let named = (north: 1, east: 2)
  let explicit: [i32; 4] = [1, 2, 3, 4]
  let repeated: [i32; 4] = [0; 4]
  let smaller: [i32; 2] = [8, 9]
  // let larger: [i32; 4] = smaller // error: count is part of the type
  let values = [1, 2, 3, 4]
  let scores = ["north": 7, "south": 9]
  let eager = values
    .filter((value) => value % 2 == 0)
    .map((value) => value * 10)
  let lazy = values.lazy
    .filter((value) => value % 2 == 0)
    .map((value) => value * 10)
    .take(2)
    .collect() // only the explicitly lazy Iterator needs materialization

  var closed = 0
  for value in 1...3 { closed += value }

  var halfOpen = 0
  for value in 1..<3 { halfOpen += value }

  let inner = values[1>..<3]
  let rightClosed = values[1>..3]
  guard let north = scores["north"] else panic("fixture key is missing")
  expect positional.1 == "north"
  expect singleton.0 == 7
  expect explicit[2] == 3
  expect repeated == [0, 0, 0, 0]
  expect smaller.count == 2
  expect eager == lazy
  return (
    named.east,
    north,
    eager[0],
    explicit.count + inner.count + rightClosed.count,
    closed + halfOpen + lazy[0],
  )
}

test "collections expose labels, bounds, and counts" for collectionSummary {
  expect collectionSummary() == (2, 7, 20, 7, 29)
}
```

## Operators and pipe-forward

<!-- w-example role=executable use=addOne,double,renderNumber,clamp,multiply,divide,remainder,Reading,operatorSummary observable=value -->
```w
fn addOne(_ value: i32): i32 { return value + 1 }
fn double(_ value: i32): i32 { return value * 2 }
fn renderNumber(_ value: i32): String { return "${value}" }
fn clamp(_ value: i32, minimum lower: i32, maximum upper: i32): i32 {
  return value.max(lower).min(upper)
}
fn multiply(_ value: i32, by factor: i32): i32 { return value * factor }
fn divide(_ value: i32, by divisor: i32): i32 { return value / divisor }
fn remainder(_ value: i32, by divisor: i32): i32 { return value % divisor }

object Reading {
  let value: i32

  fn scaled(by factor: i32): Reading {
    return Reading(value: value * factor)
  }

  fn limited(to maximum: i32): Reading {
    return Reading(value: value.min(maximum))
  }

  fn render(): String { return renderNumber(value) }
}

fn operatorSummary(): (String, u8, Bool, u8, i32, i32, i32, Bool, i32) {
  var flags: u8 = 0b0001
  flags |= 0b0100
  flags <<= 1

  let rendered = 20
    |> addOne()
    |> double()
    |> renderNumber()

  let objectFlow = Reading(value: 4)
    |> .scaled(by: 3)
    |> .limited(to: 10)
    |> .render()

  let bounded = 20
    |> clamp(minimum: 0, maximum: 12)
    |> multiply(by: 3)

  let functional = 8
    |> multiply(by: 2)
    |> divide(by: 3)
    |> remainder(by: 5)

  let relation = (flags & 0b1010) == 0b1010 && !false
  let xor = flags ^ 0b0011
  let power = 2 ** 5
  let quotient = 10 / 2
  let remainder = 10 % 3
  let rangeCheck = 2 in 1...3
  let optional: i32? = .none
  let fallback = optional ?? 7
  expect objectFlow == "10"
  expect bounded == 36
  expect functional == 0
  return (rendered, flags, relation, xor, power, quotient, remainder, rangeCheck, fallback)
}

test "operators and pipe-forward produce values" for operatorSummary {
  var assigned = 8
  assigned += 2
  assigned -= 1
  assigned *= 2
  assigned /= 3
  assigned %= 5
  assigned **= 2
  assigned <<= 1
  assigned >>= 1
  assigned &= 0b0011
  assigned ^= 0b0010
  assigned |= 0b0100

  expect operatorSummary() == ("42", 10, true, 9, 32, 5, 1, true, 7)
  expect assigned == 7
  expect (0b1000 >> 2) == 2
  expect (~0_u8) == 0xff
  expect 2 <= 2 && 3 >= 2
  expect 1 != 2 || false
}
```

## Numeric policies and bit primitives

<!-- w-example role=logical-contract -->
```w
fn numericPolicies(): (u8, u8, Bool, UInt) {
  expect (try? u8.checkedAdd(250, 10)) == .none
  expect (try? u8.checkedSubtract(2, 3)) == .none
  expect (try? u8.checkedMultiply(20, 20)) == .none
  expect (try? u8.checkedNegate(1)) == .none
  expect (try? u8.checkedDivide(7, 0)) == .none
  expect (try? u8.checkedRemainder(7, 0)) == .none
  expect (try? u8.checkedPower(4, 4)) == .none
  expect (try? u8.checkedShiftLeft(0x80, 1)) == .none
  expect (try? u8.checkedShiftRight(1, 8)) == .none

  let wrapped = u8.wrappingAdd(250, 10)
  expect wrapped == 4
  expect u8.wrappingSubtract(2, 3) == 255
  expect u8.wrappingMultiply(20, 20) == 144
  expect u8.wrappingNegate(1) == 255
  expect u8.wrappingPower(4, 4) == 0
  expect u8.wrappingShiftLeft(0x80, 1) == 0

  let saturated = u8.saturatingAdd(250, 10)
  expect saturated == 255
  expect u8.saturatingSubtract(2, 3) == 0
  expect u8.saturatingMultiply(20, 20) == 255
  expect u8.saturatingNegate(1) == 0
  expect u8.saturatingPower(4, 4) == 255

  expect u8.overflowingAdd(250, 10) == (4, true)
  expect u8.overflowingSubtract(2, 3) == (255, true)
  expect u8.overflowingMultiply(20, 20) == (144, true)
  expect u8.overflowingNegate(1) == (255, true)
  expect u8.overflowingPower(4, 4) == (0, true)

  expect i32.euclideanDivide(-7, 3) == -3
  expect i32.euclideanRemainder(-7, 3) == 2
  let carry = u8.carryingAdd(250, 5, carry: true)
  let borrow = u8.borrowingSubtract(0, 0, borrow: true)
  let full = u8.fullMultiply(16, 16)
  expect carry == (value: 0, carry: true)
  expect borrow == (value: 255, borrow: true)
  expect full == (high: 1, low: 0)

  expect u8.maskedShiftLeft(1, 9) == 2
  expect u8.maskedShiftRight(0x80, 9) == 0x40
  expect u8.logicalShiftRight(0x80, 1) == 0x40
  expect u8.rotatedLeft(0x81, 1) == 0x03
  expect u8.rotatedRight(0x81, 1) == 0xc0

  let bits = 0x16_u8.toBits()
  expect u8.fromBits(bits) == 0x16
  let bytes = 0x1234_u16.toBytes(order: .big)
  expect u16.fromBytes(bytes, order: .big) == 0x1234
  expect u8.bitWidth == 8
  expect u8.countOnes(0x16) == 3
  expect u8.countZeros(0x16) == 5
  expect u8.countLeadingZeros(0x16) == 3
  expect u8.countTrailingZeros(0x16) == 1
  expect u8.reversedBits(0x16) == 0x68
  expect u16.reversedBytes(0x1234) == 0x3412

  return (wrapped, saturated, carry.carry, u8.countOnes(wrapped ^ saturated))
}

test "numeric policies name overflow and representation" for numericPolicies {
  expect numericPolicies() == (4, 255, true, 7)
}
```

## Functions, labels, defaults, and rest

<!-- w-example role=executable use=labelled,join,route observable=value -->
```w
fn labelled(
  _ value: String,
  externalAudit audit: String,
  _ note: String,
  to destination: String,
  title: String = "city",
): String {
  return value + audit + note + destination + title
}

fn join(separator: String, values: String...): String {
  return values.joined(separator: separator)
}

fn route(_ audit: String): String { return "positional:${audit}" }
fn route(audit: String): String { return "labeled:${audit}" }

test "call labels and rest arguments keep their shape" for labelled {
  let labels = labelled("n", externalAudit: "o", "r", title: "h", to: "t")
  let values = ["east", "west"]
  expect labels == "north"
  expect join(separator: "/", values: each values) == "east/west"
  expect route("open") == "positional:open"
  expect route(audit: "open") == "labeled:open"
}
```

## Structs, objects, enums, and extensions

<!-- w-example role=executable use=Place,Counter,Signal,describe observable=value -->
```w
struct Place {
  let id: u64
  var label: String = "square"

  init(id: u64, label: String) {
    self.id = id
    self.label = label
  }

  deinit { print("dropping ${label}") }
}

object Counter {
  var value: i32
  mut fn increment() { value += 1 }
}

enum Signal {
  quiet
  alert(level: u8)
}

extension Place {
  fn describe(): String { return "${id}:${label}" }
}

fn describe(signal: Signal): String {
  return switch signal {
    case .quiet: "quiet"
    case .alert(let level): "alert:${level}"
  }
}

test "nominal declarations expose their members" for Place {
  let place = Place(id: 7, label: "north")
  let counter = Counter(value: 0)
  let signal: Signal = .alert(level: 2)
  let ref { id, ... } = place
  let (description, observedCount) = (place.describe(), counter.value)
  counter.increment()
  expect id == 7
  expect description == "7:north"
  expect observedCount == 0
  expect counter.value == 1
  expect counter.isSameInstance(as: counter)
  expect describe(signal: signal) == "alert:2"
}
```

## Protocols, generics, refinements, and specialization

<!-- w-example role=logical-contract -->
```w
protocol Source<Item> {
  fn item(at index: usize): Item
}

protocol Counted {
  fn count(): usize
}

protocol Catalog<Item>: Source<Item> & Counted {}

struct Shelf<T> {
  let items: Array<T>
}

enum Mode { fast; strict }
alias StringShelf = Shelf<String>
type AllowedMode = Mode<[.strict]>
type Digest = [u8; 32]
type SmallCount = u16<(.member <= 64)>

const DefaultColumns: usize = 4

struct StaticWindow<
  rows: usize<(.member > 0)>,
  columns: usize,
> {
  let values: [[f32; columns]; rows]
}

static const fn zeroWindow<rows: usize>(): StaticWindow<rows: rows, columns: DefaultColumns> {
  return StaticWindow(values: [[0.0; DefaultColumns]; rows])
}

extension<T: Equatable> Shelf<T>: Catalog {
  fn item(at index: usize): T { return items[index] }
  fn count(): usize { return items.count }
}

test "generic conformances preserve the concrete item" for Shelf {
  let shelf: StringShelf = Shelf(items: ["north", "south"])
  let count: SmallCount = try SmallCount(shelf.count())
  let mode: AllowedMode = .strict
  var digest: Digest = [0; 32]
  digest[0] = 0xa5
  let window = zeroWindow<rows: 2>()
  expect shelf.item(at: 1) == "south"
  expect count == 2
  expect mode == .strict
  expect digest[0] == 0xa5
  expect digest != [0; 32]
  expect window.values.count == 2
  expect window.values[0].count == DefaultColumns
}
```

## Properties, behaviors, and facets

<!-- w-example role=executable use=WrappedDegrees,Versioned,VersionedDegrees,Attitude,PropertyModes,PropertyAccessKind,accessName,nudge,overwrite observable=value -->
```w
behavior WrappedDegrees for u16 {
  var current: u16

  fn normalized(value: u16): u16 { return value % 360_u16 }

  init(initialValue: fn(): u16) { current = normalized(value: initialValue()) }
  get {
    defer { current = normalized(value: current) }
    return current
  }
  mut set(newValue) { current = normalized(value: newValue) }

  export mut fn reset() { current = 0 }
}

behavior Versioned<Value> for Value {
  var epoch: u64
  var replacements: u64
  var reads: u64

  init() {
    epoch = 0
    replacements = 0
    reads = 0
  }

  export let mutationEpoch: u64 { get => epoch }
  export let replacementCount: u64 { get => replacements }
  export let readCount: u64 { get => reads }
  export mut fn resetMutationEpoch() { epoch = 0 }

  mut willGet(kind: PropertyAccessKind) { reads += 1 }
  mut didGet(kind: PropertyAccessKind) {
    if kind == .mutableBorrowed { epoch += 1 }
  }
  mut willSet(current: ref Value, proposed: ref Value) { replacements += 1 }
  mut didSet(current: ref Value) { epoch += 1 }
}

enum PropertyAccessKind {
  value
  borrowed
  mutableBorrowed
}

object PropertyModes {
  var storage: u16 = 1

  let snapshot: u16 { get => storage }
  let borrowed: ref u16 { get => storage }
  var replaceable: u16 {
    get => storage
    set(value) => storage = value
  }
  var borrowedReplaceable: ref u16 {
    get => storage
    set(value) => storage = value
  }
  var direct: mut ref u16 { get => storage }
  var buffered: inout u16 {
    get => storage
    set(value) => storage = value
  }
}

fn accessName(kind: PropertyAccessKind): String {
  let observed: PropertyAccessKind = kind
  return switch observed {
    case .value: "value"
    case .borrowed: "borrowed"
    case .mutableBorrowed: "mutableBorrowed"
  }
}

behavior VersionedDegrees for u16 =
  (degrees: WrappedDegrees, version: Versioned)

fn nudge(value: mut ref u16) { value += 5 }
fn overwrite(value: inout u16) { value = 21 }

struct Attitude {
  var VersionedDegrees yaw: mut ref u16 = 0
  mut fn rotate(by delta: u16) { yaw += delta }
}

test "behavior composition exposes qualified facets" for Attitude {
  var modes = PropertyModes()
  let copied = modes.snapshot
  let ref borrowed = modes.borrowed
  expect copied == 1 && borrowed == 1
  modes.replaceable = 3
  modes.borrowedReplaceable = 5
  nudge(value: mut ref modes.direct)
  overwrite(value: inout modes.buffered)
  expect modes.storage == 21

  var attitude = Attitude()
  attitude.yaw = 350
  attitude.rotate(by: 25)
  expect attitude.yaw == 15
  expect attitude.yaw#version.mutationEpoch == 2
  expect attitude.yaw#version.replacementCount == 1
  expect attitude.yaw#version.readCount > 0
  attitude.yaw#degrees.reset()
  expect attitude.yaw == 0
  expect attitude.yaw#version.mutationEpoch == 3
  expect attitude.yaw#version.replacementCount == 1
  // Direct mutable-borrow access runs get observers, not set observers.
  nudge(value: mut ref attitude.yaw)
  expect attitude.yaw == 5
  expect attitude.yaw#version.mutationEpoch == 4
  expect attitude.yaw#version.replacementCount == 1
  let accessKind: PropertyAccessKind = .value
  expect accessName(kind: accessKind) == "value"
}
```

The property declaration selects the access mode; the accessor is always
spelled `get`. `let p: T` returns a read-only value, and `let p: ref T` returns
a read-only borrow. `var p: T` may replace a value, `var p: ref T` may replace
or borrow it, `var p: mut ref T` exposes a scoped exclusive borrow, and
`var p: inout T` performs copy-in/copy-out through `get` plus `set`. The forms
`let p: mut ref T`, `let p: inout T`, `get ref`, and `get mut ref` are invalid.
Every stored or computed property starts with `let`, `var`, or `const`; W does
not accept a bare `name: T` property. Enum payload labels, tuple labels,
parameters, and call labels are not properties and therefore do not use a
property binder. Neither do `build.w` manifest keys or foreign ABI layout
members; `foreign c { struct Header { size: c.size } }` describes C layout,
not a W property.
`willGet` and `didGet` are opt-in observer hooks. Their
`PropertyAccessKind` is `.value`, `.borrowed`, or `.mutableBorrowed`. Set
observers count value-in/value-out writeback, while direct mutable-borrow
access does not invoke `willSet` or `didSet`.

## Option, conversion, and type queries

<!-- w-example role=executable use=ReservationKey,LookupResult,inspectKey,metadataSummary,nameOr observable=value -->
```w
struct ReservationKey: Hashable & Reflectable {
  let orderId: u64
}

enum LookupResult: Reflectable {
  found(id: u64)
  missing
}

fn inspectKey(value: ref any Hashable): u64? {
  if value is ReservationKey {
    if let ref key = value as? ReservationKey {
      let staticId = type of ReservationKey
      let dynamicId = type of value
      let ref metadata = info of ReservationKey
      guard metadata.id == staticId && dynamicId == staticId else { return .none }
      return .some(key.orderId)
    }
  }
  return .none
}

fn metadataSummary(): (TypeId, String, TypeKind, TypeId?, usize, TypeId, String) {
  let ref metadata = info of ReservationKey
  let ref property = metadata.properties[0]
  let ref enumMetadata = info of LookupResult
  let ref foundCase = enumMetadata.cases[0]
  return (
    metadata.id,
    copy metadata.name,
    metadata.kind,
    metadata.base,
    metadata.properties.count,
    property.valueType,
    copy foundCase.name,
  )
}

fn nameOr(value: String?): String {
  return value?.trim() ?? "unknown"
}

test "conditional cast keeps the borrowed value" for ReservationKey {
  let key = ReservationKey(orderId: 42)
  let missing: LookupResult = .missing
  let summary = metadataSummary()
  expect inspectKey(value: ref key) == .some(42)
  expect summary.0 == type of ReservationKey
  expect !summary.1.isEmpty
  expect summary.2 == .struct
  expect summary.3 == .none
  expect summary.4 == 1
  expect summary.5 == type of u64
  expect summary.6 == "found"
  let ref keyInfo = info of ReservationKey
  let ref orderId = keyInfo.properties[0]
  let ref resultInfo = info of LookupResult
  let ref found = resultInfo.cases[0]
  expect orderId.name == "orderId"
  expect orderId.mutability == .immutable
  expect orderId.accessMode == .value
  expect !orderId.hasSetter
  expect found.payloadTypes == [type of u64]
  expect resultInfo.cases[1].payloadTypes.isEmpty
  expect missing == .missing
  expect nameOr(value: .some(" W ")) == "W"
  expect nameOr(value: .none) == "unknown"
}
```

## Ownership, borrows, and views

<!-- w-example role=executable use=Point,Ticket,Receipt,translated,ticketLabel,bumpTicket,consumeTicket,readFirst,replaceFirst,consume,window observable=value -->
```w
struct Point: Copy & Equatable {
  let x: i32
  let y: i32
}

struct Receipt {
  let id: u64
}

object Ticket {
  var label: String
}

fn translated(point: Point): Point {
  return Point(x: point.x + 1, y: point.y + 1)
}

fn ticketLabel(ticket: Ticket): String { return copy ticket.label }
fn bumpTicket(ticket: mut ref Ticket) { ticket.label = "bumped" }
fn consumeTicket(ticket: take Ticket): String { return copy ticket.label }

fn readFirst(values: ref Array<String>): String { return values[0] }

fn replaceFirst(values: inout Array<String>, replacement: String) {
  values[0] = replacement
}

fn consume(value: take String): String { return value }

fn window(values: view Array<String>): view Array<String> {
  return values[1..<3]
}

test "ownership operations are explicit at the call site" for consume {
  let point = Point(x: 1, y: 2)
  let translatedPoint = translated(point: point) // Point is Copy; the value call is implicit.
  var ticket = Ticket(label: "T-7")
  expect ticketLabel(ticket: ticket) == "T-7" // object parameters default to ref.
  bumpTicket(ticket: mut ticket) // `mut objectPlace` is the short form for mut ref.
  bumpTicket(ticket: mut ref ticket) // explicit spelling remains valid.
  let ticketText = consumeTicket(ticket: take ticket)

  var values = ["north", "east", "south"]
  let copied = copy readFirst(values: ref values)
  replaceFirst(values: inout values, replacement: "west")
  let pinned = pin values
  let middle = window(values: values)
  let moved = consume(value: take copied)
  let receipt = Receipt(id: 7)
  let movedReceipt = receipt // non-Copy values move at last use.
  let _ = pinned
  expect point == Point(x: 1, y: 2)
  expect translatedPoint == Point(x: 2, y: 3)
  expect ticketText == "T-7"
  expect moved == "north"
  expect movedReceipt.id == 7
  expect values[0] == "west"
  expect middle.count == 2
}
```

`ref T` is a shared read-only borrow. `mut ref T` is a dependent exclusive
borrow. `mut view T` is an exclusive logical view. `inout T` is only a
parameter/call convention: `values: inout values` reserves the source place,
lets the callee mutate a local, and writes back on normal return or structured
`throw`. It is not a field, result, binding mode, or iteration mode.

Structs and enums are value-semantic and are not automatically `Copy`. `Copy`
is implicit, bounded, and has no hidden allocation or data-dependent graph
traversal; fixed fieldwise traversal of `Copy` fields is allowed. `Duplicable` is
an explicit `copy value` contract that may allocate or traverse and promises
logical independence. In graph terms, `Copy` is always shallow: it never clones
the reachable object graph. A statically bounded traversal of inline `Copy`
fields does not make it a deep copy. An `object` is a singular identity/owner and cannot satisfy `Copy`; sharing uses a
`shared` handle, while `Duplicable` must create a valid new identity. A type may
declare a first-party COW strategy for `Duplicable`, but COW is not a universal
String or Array baseline and must document allocator, budget, failure, cleanup,
and cross-domain costs. On a computed property, the surface spelling is
`set(value)`; a behavior body may write `mut set(value)` to mark mutation of its
backing storage.

## Callable values and captures

<!-- w-example role=executable use=CaptureBox,captures observable=value -->
```w
object CaptureBox {
  let value: String
}

fn captures(
  _ copied: String,
  _ borrowed: ref String,
  _ moved: take String,
  _ sharedValue: shared CaptureBox,
): (String, String, String, String?, usize, String) {
  let copyClosure: some fn(): String = <[copy copied]>() => copied
  let refClosure: some fn(): String = <[ref borrowed]>() => borrowed
  let takeClosure: some take fn(): String = <[take moved]>() => moved
  let weakClosure = <[weak sharedValue]>() => if let owner = sharedValue {
    .some(copy owner.value)
  } else {
    .none
  }
  var next: usize = 0
  var sequence: some mut fn(): usize = <[take next]>() => {
    next += 1
    return next
  }
  let erased: any fn(String): String =
    <[copy copied]>(value) => value + copied

  return (
    copyClosure(),
    refClosure(),
    (take takeClosure)(),
    weakClosure(),
    sequence(),
    erased("erased:"),
  )
}

test "capture lists preserve ownership modes" for captures {
  let borrowed = "borrowed"
  let moved = "moved"
  let box: shared CaptureBox = CaptureBox(value: "shared")
  expect captures("copied", ref borrowed, take moved, box)
    == ("copied", "borrowed", "moved", .some("shared"), 1, "erased:copied")
}
```

## Control flow and patterns

<!-- w-example role=executable use=Signal,classify,accumulate observable=value -->
```w
enum Signal {
  quiet
  alert(level: u8)
}

fn classify(_ signal: Signal): String {
  return switch signal {
    case .quiet: "quiet"
    case .alert(let level) if level > 0: "alert"
    case .alert(_): "silent-alert"
  }
}

fn accumulate(_ rows: Array<Array<i32>>): i32 {
  var total = 0
  matrixRows: for row in rows {
    for value in row {
      if value < 0 { continue matrixRows }
      if value > 50 { break matrixRows }
      total += value
    }
  }

  var attempts = 0
  while attempts < 2 { attempts += 1 }
  repeat { total += 1 } while total < 4

  capped: {
    if total <= 10 { break capped }
    total = 10
  }

  return if total > 0 { total } else { 0 }
}

test "control flow returns an observable value" for accumulate {
  let signal: Signal = .alert(level: 1)
  expect classify(signal) == "alert"
  expect accumulate([[1, 2], [-1, 100], [3]]) == 6
}
```

## Errors and cleanup

<!-- w-example role=executable use=ParseError,positive,parseAndClose,asyncCleanup,immediate observable=value -->
```w
enum ParseError: Error {
  negative
}

fn positive(_ value: i32): i32 throws ParseError {
  guard value >= 0 else { throw .negative }
  return value
}

fn parseAndClose(_ value: i32, closed: inout Bool): i32 {
  defer { closed = true }
  do {
    return try positive(value)
  } catch .negative {
    return 0
  }
}

async fn asyncCleanup(_ value: i32): i32 {
  defer async { await execution#yield() }
  return value
}

async fn immediate(_ value: i32): i32 { return value }

test "do/catch handles typed errors and defer closes" for parseAndClose {
  let error: ParseError = .negative
  var closed = false
  expect error == .negative
  expect parseAndClose(-1, closed: inout closed) == 0
  expect closed
  expect (try? positive(-1)) == .none
  expect sync immediate(7) == 7
  expect await asyncCleanup(42) == 42
}
```

`sync asyncCleanup(42)` is rejected: its asynchronous cleanup means that the
function has no proven direct entry. `sync` never blocks and never drives a
task to completion.

## Allocator scopes

<!-- w-example role=executable use=stage,prepare,edit observable=value -->
```w
fn stage(city: String, allocator destination: ref Allocator): String {
  return city
}

fn edit(value: inout String) { value.append("!") }

fn prepare(city: String): (String, usize) {
  var result = city
  var bytes: usize = 0

  allocator scratch: .fixed<capacity: 256> {
    var copyOfCity = city
    edit(value: inout copyOfCity)
    result = stage(city: copyOfCity, allocator: ref scratch)
  }

  allocator .fixed<capacity: 128> {
    bytes = result.bytes.count
  }

  return (result, bytes)
}

test "allocator scopes bound temporary work" for prepare {
  expect prepare("city") == ("city!", 5)
}
```

## Unsafe, addresses, and bit operations

<!-- w-example role=executable use=clearTag observable=value -->
```w
unsafe fn clearTag(_ pointer: Address<.virtual, .readWrite>, tagMask: usize): usize {
  let alignedBits = pointer.bits & ~tagMask
  let aligned = pointer.withAddress(alignedBits)
  return aligned.bits
}

test "address arithmetic stays inside unsafe" for clearTag {
  let address = Address<.virtual, .readWrite>.fromBits(0x1003)
  let bits = unsafe { clearTag(address, tagMask: 0x0003) }
  expect bits == 0x1000
}
```

## Async, spawn, sync, and await

<!-- w-example role=executable use=FetchError,fetch,ordinary,load observable=value -->
```w
enum FetchError: Error { unavailable }

async fn fetch(_ city: String): String throws FetchError {
  return city
}

fn ordinary(): String { return "local" }

async fn load(): String throws FetchError {
  let direct = try sync fetch("north")
  let concurrent = async fetch("east")
  let parallel = spawn<.network> fetch("south")
  let local = async ordinary()
  let (east, south) = try await (concurrent, parallel)

  return direct
    + east
    + south
    + (await local)
}

test "launchers join through the lexical parent" for load {
  let error: FetchError = .unavailable
  expect error == .unavailable
  expect try await load() == "northeastsouthlocal"
}
```

## Tasks and cancellation

<!-- w-example role=logical-contract -->
```w
enum WorkError: Error { failed }

alias TextTask = Task<String, WorkError>
alias TextOutcome = TaskOutcome<String, WorkError>
alias TextSettlement = TaskSettlement<String, WorkError>

async fn work(_ value: String): String throws WorkError { return value }

struct Trace {
  const requestId = TaskLocal<String?>.key(default: .none)
}

async fn traced(_ value: String): String throws WorkError {
  return try await Trace.requestId.withValue(
    .some("request-42"),
    operation: () => try await work(value + Trace.requestId.get()?),
  )
}

async fn cancelAndObserve(): TextOutcome {
  let child: TextTask = async work("cancelable")
  child#cancel(reason: .shutdown)
  return await (take child)#outcome()
}

async fn timed(_ value: String, timeout: TaskTimeout): TextOutcome {
  return await Task.withTimeout(
    for: timeout,
    input: value,
    using: work,
  )
}

async fn first(): TextSettlement {
  let primary: TextTask = async work("primary")
  let fallback: TextTask = spawn<.compute> work("fallback")
  let candidates: [TextTask; 2] = [primary, fallback]
  return await (take candidates).firstSettled()
}

test "firstSettled preserves index and outcome" for first {
  let error: WorkError = .failed
  let settlement = await first()
  let tracedValue = try await traced("value:")
  let canceled = await cancelAndObserve()
  let timedValue = await timed("bounded", timeout: 250<si.ms>)
  expect error == .failed
  expect tracedValue == "value:request-42"
  expect settlement.index < 2
  expect switch settlement.outcome {
    case .success(let value): value == "primary" || value == "fallback"
    case .error(_): false
    case .canceled(_): false
  }
  expect switch canceled {
    case .success(_): true
    case .error(_): false
    case .canceled(_): true
  }
  expect switch timedValue {
    case .success(let value): value == "bounded"
    case .error(_): false
    case .canceled(_): true
  }
}
```

## Bounded task pipelines

<!-- w-example role=executable use=JobError,process,processAll,collectAll observable=value -->
```w
enum JobError: Error { failed }

async fn process(_ value: i32): i32 throws JobError { return value * 2 }

async fn processAll(_ values: take Array<i32>): Array<i32> throws JobError {
  return try await pipeline<
    tasks: .parallel<.compute>,
    limit: 4,
    ordering: .input,
    errors: .failFast,
  > each value in take values {
    commit try process(value)
  }
}

async fn collectAll(
  _ values: take Array<i32>,
): Array<TaskSettlement<i32, JobError>> throws JobError {
  return try await pipeline<
    tasks: .concurrent,
    limit: 4,
    ordering: .completion,
    errors: .collect,
  > each value in take values {
    commit try process(value)
  }
}

test "bounded task pipeline preserves input order" for processAll {
  let error: JobError = .failed
  let settlements = try await collectAll([1, 2, 3])
  expect error == .failed
  expect try await processAll([1, 2, 3]) == [2, 4, 6]
  expect settlements.count == 3
}
```

## Service and transaction pipelines

<!-- w-example role=logical-contract -->
```w
struct OvenLease {
  let temperature: u16
  fn preheat(): u16 { return temperature }
}

protocol OvenApi {
  fn acquire(temperature: u16): OvenLease
}

service ovens<key: String>: OvenApi {
  fn acquire(temperature: u16): OvenLease {
    return OvenLease(temperature: temperature)
  }
}

protocol StoreApi {
  fn read(): String
}

service stores<key: String>: StoreApi {
  fn read(): String { return "north" }
}

async fn prepare(): (u16, String) {
  let oven = ovens.at("primary")
  let store = stores.at("menu")
  let ready = await pipeline oven.acquire(temperature: 220).preheat()
  let value = try await pipeline<transaction: {
    isolation: .serializable,
    access: .readOnly,
  }> tx = store {
    commit tx.read()
  }
  return (ready, value)
}

test "pipeline commits the terminal value" for prepare {
  expect await prepare() == (220, "north")
}
```

The product chooses whether the same service contract is linked locally, as a
component, through IPC, or through the network:

<!-- w-example role=logical-contract -->
```w
// excerpt-kind: manifest-fragment
products: [
  {
    name: "kitchen"
    modules: ["kitchen"]
    servicePolicy: {
      resolution: .startup
      links: [
        .local,
        .component,
        .wrpc(transports: [.ipc, .network]),
      ]
      dynamicRebinding: .deny
    }
    services: [
      {
        binding: "ovens"
        declaration: "kitchen::ovens"
        scope: .process
        mailbox: { items: 64, bytes: 8MiB, inFlight: 1 }
      },
    ]
  },
]
```

## Streams and channels

<!-- w-example role=executable use=StreamError,project,relay observable=value -->
```w
enum StreamError: Error { closed }

fn project(_ source: take Stream<String, Never>): some Stream<String, Never> {
  return stream <[take source]> {
    var cursor = take source
    while let item = await cursor.next() {
      yield copy item
    }
  }
}

async fn relay(
  _ source: take Stream<String, StreamError>,
  _ sender: take Channel<send: String>,
): usize throws StreamError {
  var count: usize = 0
  for try await item in take source {
    await sender.send(take item)
    count += 1
  }
  await sender.close()
  return count
}

test "stream projection remains lazy" for project {
  let error: StreamError = .closed
  let source = Stream.from(["north", "south"])
  let projected = project(take source)
  let (sender, receiver) = Channel<String>.open(capacity: 2)
  let received = async receiver.receive()
  let relayed = async relay(Stream.from(["east"]), take sender)
  expect error == .closed
  expect await projected.collect() == ["north", "south"]
  expect try await relayed == 1
  expect await received == "east"
}
```

## Shared state, atomics, and locks

<!-- w-example role=executable use=Ledger,Published,publish observable=value -->
```w
object Ledger {
  var atomic count: usize = 0
  var message: String = ""
}

struct Published: Duplicable {
  let revision: u64
  let value: String
}

fn publish(_ ledger: shared Ledger, _ message: String): (usize, u64) {
  ledger.count.saturatingAdd<.relaxed>(1)
  lock ledger as exclusive {
    exclusive.message = message
  }
  let snapshots = SnapshotCell(Published(revision: 1, value: message))
  snapshots.publish(Published(revision: 2, value: message))
  let revision = snapshots.read((value: ref Published) => value.revision)
  return (ledger.count.load<.acquire>(), revision)
}

test "atomic and lock operations expose their ordering" for Ledger {
  let ledger: shared Ledger = Ledger()
  expect publish(ledger, "stored") == (1, 2)
  expect ledger.message == "stored"
}
```

## Units, matrices, tensors, and SIMD

<!-- w-example role=logical-contract -->
```w
import accelerator from std
import { Tensor } from std.tensor

dimension Distance
unit kilometer: Distance

type FeatureBatch<rows: usize, columns: usize> =
  Tensor<f32, shape: [rows, columns]>

fn forecastKernel<rows: usize, inputs: usize, outputs: usize>(
  _ features: ref FeatureBatch<rows: rows, columns: inputs>,
  weights: ref Tensor<f32, shape: [inputs, outputs]>,
): FeatureBatch<rows: rows, columns: outputs> {
  return features @ weights
}

const kernels = accelerator.module<{
  forecast: forecastKernel,
}>()

fn numericSummary(): (Quantity<Distance>, i32, i32, [usize; 2], i32, i32, i32) {
  let distance = 12<kilometer>
  let matrix = [[1, 2], [3, 4]]
  let vector = Simd<i32, lanes: 4>([1, 2, 3, 4])
  let doubled = vector + vector
  let sum = vector.wrappingReduceAdd()
  let product = vector.saturatingReduceMultiply()
  let xor = vector.reduceBitXor()
  let features: FeatureBatch<rows: 1, columns: 2> = [[1.0, 2.0]]
  let weights: Tensor<f32, shape: [2, 1]> = [[1.0], [0.5]]
  let result = forecastKernel(features, weights: weights)
  let _ = kernels
  return (distance, matrix[1][0], doubled[3], result.shape, sum, product, xor)
}

test "numeric types preserve dimensions and lanes" for numericSummary {
  let result = numericSummary()
  expect result.0 == 12<kilometer>
  expect result.1 == 3
  expect result.2 == 8
  expect result.3 == [1, 1]
  expect result.4 == 10
  expect result.5 == 24
  expect result.6 == 4
}
```

## Foreign code and ABI

<!-- w-example role=logical-contract -->
```w
foreign c from "stdlib.h" {
  fn abs(_ value: c.int): c.int
}

export foreign c {
  struct w_result { value: c.int }
}

export unsafe fn<abi: .c> w_add(_ left: c.int, _ right: c.int): w_result {
  return w_result(value: left + right)
}

unsafe fn<lang: .c> c_add(_ left: c.int, _ right: c.int): c.int {
  return left + right;
}

unsafe fn callC(_ left: i32, _ right: i32): i32 {
  return abs(c_add(left, right))
}

test "foreign calls remain inside unsafe" for callC {
  expect unsafe { callC(20, 22) } == 42
  expect unsafe { w_add(20, 22).value } == 42
}
```

## Packages and workspaces

`build.w` is data, so it stays separate from module source. A standalone
package needs only a `package` root:

<!-- w-example role=logical-contract -->
```w
// excerpt-kind: manifest-fragment
package {
  schema: "w.package/1"
  name: "last-light"
  version: "0.1.0"
  edition: "2026"
  moduleSets: [{ name: "app", root: "src", include: ["*.w"] }]
  products: [{
    name: "last-light-native"
    kind: .executable
    module: "app"
    entry: "LastLightTui"
  }]
}
```

An aggregate manifest may contain only a `workspace` root. A member can also be
the root package by using `"."`:

<!-- w-example role=logical-contract -->
```w
// excerpt-kind: manifest-fragment
workspace {
  schema: "w.workspace/1"
  members: [".", "packages/core", "packages/server"]
  defaultMembers: ["."]
  patches: []
}
```

A root that is both publishable and an aggregate writes one `package` record
and one `workspace` record in the same `build.w`; their order is irrelevant.

```text
w check                         # checks the current package or workspace defaults
w check --package core          # selects one workspace member
w run last-light-native -- --tui
```

The normative contract and implementation status remain in [DESIGN.md](DESIGN.md).
