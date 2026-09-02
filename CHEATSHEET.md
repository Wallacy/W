# W language cheatsheet

> Design draft. Every example is a complete syntax unit: it declares a feature,
> uses it, and shows an observable result. Some units are design/source evidence
> until the corresponding compiler or runtime feature exists.

## Contents

- [Program and entry](#program-and-entry)
- [Modules, imports, exports, and reexports](#modules-imports-exports-and-reexports)
- [Documentation and tests](#documentation-and-tests)
- [Literals and interpolation](#literals-and-interpolation)
- [Collections and ranges](#collections-and-ranges)
- [Operators and pipe-forward](#operators-and-pipe-forward)
- [Numeric policies and bit primitives](#numeric-policies-and-bit-primitives)
- [Functions, labels, defaults, and rest](#functions-labels-defaults-and-rest)
- [Structs, objects, enums, and extensions](#structs-objects-enums-and-extensions)
- [Protocols, generics, and static contracts](#protocols-generics-and-static-contracts)
- [Compile-time values and specialization](#compile-time-values-and-specialization)
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
- [Execution context and process entry](#execution-context-and-process-entry)
- [Units, matrices, tensors, and SIMD](#units-matrices-tensors-and-simd)
- [Foreign code and ABI](#foreign-code-and-abi)
- [Packages and workspaces](#packages-and-workspaces)

## Program and entry

<!-- w-example role=executable use=main observable=effect -->
```w
module hello

fn main() {
  print("Hello, world!")
}

entry(main)
```

## Modules, imports, exports, and reexports

<!-- w-example role=executable use=greeting observable=value -->
```w
module greeting<
  domains: [.serial],
>

import text from std
import { String as Text } from std.text
import * from std.memory
export * from greeting.domain
export { Place as PublicPlace } from greeting.domain

export fn greeting(name: Text): Text {
  return text.join(["Hello, ", name])
}

test "imports resolve names used by exported declarations" for greeting {
  expect greeting("W") == "Hello, W"
}
```

## Documentation and tests

<!-- w-example role=executable use=clamp observable=value -->
```w
/// Limits a value to an inclusive interval.
///
/// @example
/// call: clamp(4, minimum: 0, maximum: 3)
/// result: 3
fn clamp(value: i32, named minimum: i32, named maximum: i32): i32 {
  return value.max(minimum).min(maximum)
}

test "clamp preserves an internal value" for clamp {
  expect clamp(2, minimum: 0, maximum: 3) == 2
}
```

## Literals and interpolation

<!-- w-example role=executable use=literalSummary observable=value -->
```w
fn literalSummary(seconds: u64): String {
  let integer: i32 = 1_000
  let hexadecimal = 0xff
  let ratio: f64 = 0.5e2
  let enabled: Bool = true
  let scalar: UnicodeScalar = 'λ'
  let byte: u8 = b'W'
  let byteString = b"W"
  let text = "${integer}:${hexadecimal}:${ratio}:${enabled}:${scalar}:${byte}"
  let raw = #"{"value":#${seconds},"unit":"s"}"#
  let literalMarker = #"${seconds}"#
  let multiline = """north
south"""
  return raw + literalMarker + multiline + text + "${byteString.count}"
}

test "raw strings opt into interpolation" for literalSummary {
  expect literalSummary(30).starts(with: #"{"value":30,"unit":"s"}"#)
  expect literalSummary(30).contains(#"${seconds}"#)
}
```

## Collections and ranges

<!-- w-example role=executable use=collectionSummary observable=value -->
```w
fn collectionSummary(): (i32, i32, usize, i32) {
  let tuple = (north: 1, east: 2)
  let fixed: [i32; 4] = [0; 4]
  let values = [1, 2, 3, 4]
  let scores = ["north": 7, "south": 9]
  let selected = values.lazy
    .filter((value) => value % 2 == 0)
    .map((value) => value * 10)
    .take(2)
    .collect()

  var closed = 0
  for value in 1...3 { closed += value }

  var halfOpen = 0
  for value in 1..<3 { halfOpen += value }

  let inner = values[1>..<3]
  let rightClosed = values[1>..3]
  guard let north = scores["north"] else panic("fixture key is missing")
  return (tuple.east, north, fixed.count + inner.count + rightClosed.count, closed + halfOpen + selected[0])
}

test "collections expose labels, bounds, and counts" for collectionSummary {
  expect collectionSummary() == (2, 7, 7, 29)
}
```

## Operators and pipe-forward

<!-- w-example role=executable use=addOne,double,renderNumber,operatorSummary observable=value -->
```w
fn addOne(value: i32): i32 { return value + 1 }
fn double(value: i32): i32 { return value * 2 }
fn renderNumber(value: i32): String { return "${value}" }

fn operatorSummary(): (String, u8, Bool, u8, i32, i32, i32, Bool, i32) {
  var flags: u8 = 0b0001
  flags |= 0b0100
  flags <<= 1

  let rendered = 20
    |> addOne()
    |> double()
    |> renderNumber()

  let relation = (flags & 0b1010) == 0b1010 && !false
  let xor = flags ^ 0b0011
  let power = 2 ** 5
  let quotient = 10 / 2
  let remainder = 10 % 3
  let rangeCheck = 2 in 1...3
  let optional: i32? = .none
  let fallback = optional ?? 7
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
fn numericPolicies(value: u16, other: u16): (u16, u16, usize) {
  let checkedAdd = u16.checkedAdd(value, other)
  let checkedSubtract = u16.checkedSubtract(value, other)
  let checkedMultiply = u16.checkedMultiply(value, other)
  let checkedNegate = u16.checkedNegate(value)
  let checkedDivide = u16.checkedDivide(value, other)
  let checkedRemainder = u16.checkedRemainder(value, other)
  let checkedPower = u16.checkedPower(value, other)
  let checkedShiftLeft = u16.checkedShiftLeft(value, other)
  let checkedShiftRight = u16.checkedShiftRight(value, other)

  let wrapped = u16.wrappingAdd(value, other)
  let wrappingSubtract = u16.wrappingSubtract(value, other)
  let wrappingMultiply = u16.wrappingMultiply(value, other)
  let wrappingNegate = u16.wrappingNegate(value)
  let wrappingPower = u16.wrappingPower(value, other)
  let wrappingShiftLeft = u16.wrappingShiftLeft(value, other)

  let saturated = u16.saturatingAdd(value, other)
  let saturatingSubtract = u16.saturatingSubtract(value, other)
  let saturatingMultiply = u16.saturatingMultiply(value, other)
  let saturatingNegate = u16.saturatingNegate(value)
  let saturatingPower = u16.saturatingPower(value, other)

  let overflow = u16.overflowingAdd(value, other)
  let overflowingSubtract = u16.overflowingSubtract(value, other)
  let overflowingMultiply = u16.overflowingMultiply(value, other)
  let overflowingNegate = u16.overflowingNegate(value)
  let overflowingPower = u16.overflowingPower(value, other)

  let euclideanDivide = i32.euclideanDivide(-7, 3)
  let euclideanRemainder = i32.euclideanRemainder(-7, 3)
  let carry = u16.carryingAdd(value, other)
  let borrow = u16.borrowingSubtract(value, other)
  let full = u16.fullMultiply(value, other)
  let maskedLeft = u16.maskedShiftLeft(value, other)
  let maskedRight = u16.maskedShiftRight(value, other)
  let logicalRight = u16.logicalShiftRight(value, other)
  let rotatedLeft = u16.rotatedLeft(value, other)
  let rotatedRight = u16.rotatedRight(value, other)

  let bits = value.toBits()
  let fromBits = u16.fromBits(bits)
  let bytes = value.toBytes(order: .big)
  let fromBytes = u16.fromBytes(bytes, order: .big)
  let bitWidth = u16.bitWidth
  let countOnes = u16.countOnes(value)
  let countZeros = u16.countZeros(value)
  let countLeadingZeros = u16.countLeadingZeros(value)
  let countTrailingZeros = u16.countTrailingZeros(value)
  let reversedBits = u16.reversedBits(value)
  let reversedBytes = u16.reversedBytes(value)

  let _ = checkedAdd
  let _ = checkedSubtract
  let _ = checkedMultiply
  let _ = checkedNegate
  let _ = checkedDivide
  let _ = checkedRemainder
  let _ = checkedPower
  let _ = checkedShiftLeft
  let _ = checkedShiftRight
  let _ = wrappingSubtract
  let _ = wrappingMultiply
  let _ = wrappingNegate
  let _ = wrappingPower
  let _ = wrappingShiftLeft
  let _ = saturatingSubtract
  let _ = saturatingMultiply
  let _ = saturatingNegate
  let _ = saturatingPower
  let _ = overflow
  let _ = overflowingSubtract
  let _ = overflowingMultiply
  let _ = overflowingNegate
  let _ = overflowingPower
  let _ = euclideanDivide
  let _ = euclideanRemainder
  let _ = carry
  let _ = borrow
  let _ = full
  let _ = maskedLeft
  let _ = maskedRight
  let _ = logicalRight
  let _ = rotatedLeft
  let _ = rotatedRight
  let _ = fromBits
  let _ = fromBytes
  let _ = countOnes
  let _ = countZeros
  let _ = countLeadingZeros
  let _ = countTrailingZeros
  let _ = reversedBits
  let _ = reversedBytes
  return (wrapped, saturated, bitWidth)
}

test "numeric policies name overflow and representation" for numericPolicies {
  expect numericPolicies(8, 2) == (10, 10, 16)
}
```

## Functions, labels, defaults, and rest

<!-- w-example role=executable use=labelled,join observable=value -->
```w
fn labelled(
  value: String,
  named audit: String,
  _ note: String,
  to destination: String,
  title: String = "city",
): String {
  return value + audit + note + destination + title
}

fn join(separator: String, each values: String...): String {
  return values.joined(separator: separator)
}

test "call labels and rest arguments keep their shape" for labelled {
  let labels = labelled("n", audit: "o", "r", to: "t", title: "h")
  let values = ["east", "west"]
  expect labels == "north"
  expect join("/", each values) == "east/west"
}
```

## Structs, objects, enums, and extensions

<!-- w-example role=executable use=Place,Counter,Signal,describe observable=value -->
```w
struct Place {
  id: u64
  var label: String = "square"

  init(id: u64, label: String) {
    self.id = id
    self.label = label
  }

  deinit { label }
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
  let Place(id, ...) = place
  let (description, observedCount) = (place.describe(), counter.value)
  counter.increment()
  expect id == 7
  expect description == "7:north"
  expect observedCount == 0
  expect counter.value == 1
  expect counter.isSameInstance(as: counter)
  expect describe(signal) == "alert:2"
}
```

## Protocols, generics, and static contracts

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
  items: Array<T>
}

enum Mode { fast; strict }
alias StringShelf = Shelf<String>
type AllowedMode = Mode<[.strict]>
type Digest = [u8; 32]

extension<T: Equatable> Shelf<T>: Catalog {
  fn item(at index: usize): T { return items[index] }
  fn count(): usize { return items.count }
}

const fn isSmall(value: u16): Bool { return value <= 64 }
type SmallCount = u16<(isSmall(.member))>

test "generic conformances preserve the concrete item" for Shelf {
  let shelf: StringShelf = Shelf(items: ["north", "south"])
  let count: SmallCount = try SmallCount(shelf.count())
  let mode: AllowedMode = .strict
  let digest: Digest = [0; 32]
  expect shelf.item(at: 1) == "south"
  expect count == 2
  expect mode == .strict
  expect digest.count == 32
}
```

## Compile-time values and specialization

<!-- w-example role=logical-contract -->
```w
const DefaultColumns: usize = 4

const fn isPositive(value: usize): Bool { return value > 0 }

struct StaticWindow<
  rows: usize<(isPositive(.member))>,
  columns: usize,
> {
  values: [[f32; columns]; rows]
}

static const fn zeroWindow<rows: usize>(): StaticWindow<rows: rows, columns: DefaultColumns> {
  return StaticWindow(values: [[0.0; DefaultColumns]; rows])
}

test "static values select a finite specialization" for zeroWindow {
  let window = zeroWindow<rows: 2>()
  expect window.values.count == 2
  expect window.values[0].count == DefaultColumns
}
```

## Properties, behaviors, and facets

<!-- w-example role=executable use=WrappedDegrees,Versioned,VersionedDegrees,Attitude observable=value -->
```w
behavior WrappedDegrees for u16 {
  var current: u16

  init(initialValue: fn(): u16) { current = initialValue() % 360_u16 }
  get { return current }
  mut set(newValue) { current = newValue % 360_u16 }
  mut modify {
    defer { current %= 360_u16 }
    return inout current
  }

  export mut fn reset() { current = 0 }
}

behavior Versioned<Value> for Value {
  var epoch: u64
  init() { epoch = 0 }
  export mutationEpoch: u64 { get => epoch }
  export mut fn resetMutationEpoch() { epoch = 0 }
  willSet(current: ref Value, proposed: ref Value) {}
  mut didSet(current: ref Value) { epoch += 1 }
  willModify(current: ref Value) {}
  mut didModify(current: ref Value) { epoch += 1 }
}

behavior VersionedDegrees for u16 =
  (degrees: WrappedDegrees, version: Versioned)

struct Attitude {
  var VersionedDegrees yaw: u16 = 0
  mut fn rotate(by delta: u16) { yaw += delta }
}

test "behavior composition exposes qualified facets" for Attitude {
  var attitude = Attitude()
  attitude.yaw = 350
  attitude.rotate(by: 25)
  expect attitude.yaw == 15
  expect attitude.yaw#version.mutationEpoch == 2
  attitude.yaw#degrees.reset()
  expect attitude.yaw == 0
}
```

## Option, conversion, and type queries

<!-- w-example role=executable use=ReservationKey,inspectKey,metadataSummary,nameOr observable=value -->
```w
struct ReservationKey: Hashable & Reflectable {
  orderId: u64
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

fn metadataSummary(): (TypeId, String, TypeKind, usize, usize, TypeId) {
  let ref metadata = info of ReservationKey
  let ref property = metadata.properties[0]
  return (
    metadata.id,
    copy metadata.name,
    metadata.kind,
    metadata.properties.count,
    metadata.cases.count,
    property.valueType,
  )
}

fn nameOr(value: String?): String {
  return value?.trim() ?? "unknown"
}

test "conditional cast keeps the borrowed value" for ReservationKey {
  let key = ReservationKey(orderId: 42)
  let summary = metadataSummary()
  expect inspectKey(ref key) == .some(42)
  expect summary.0 == type of ReservationKey
  expect !summary.1.isEmpty
  expect summary.2 == .struct
  expect summary.3 == 1
  expect summary.4 == 0
  expect summary.5 == type of u64
  expect nameOr(.some(" W ")) == "W"
  expect nameOr(.none) == "unknown"
}
```

## Ownership, borrows, and views

<!-- w-example role=executable use=readFirst,replaceFirst,consume,window observable=value -->
```w
fn readFirst(values: ref Array<String>): String { return values[0] }

fn replaceFirst(values: inout Array<String>, named replacement: String) {
  values[0] = replacement
}

fn consume(value: take String): String { return value }

fn window(values: view Array<String>): view Array<String> {
  return values[1..<3]
}

test "ownership operations are explicit at the call site" for consume {
  var values = ["north", "east", "south"]
  let copied = copy readFirst(ref values)
  replaceFirst(inout values, replacement: "west")
  let pinned = pin values
  let middle = window(values)
  let moved = consume(take copied)
  let _ = pinned
  expect moved == "north"
  expect values[0] == "west"
  expect middle.count == 2
}
```

## Callable values and captures

<!-- w-example role=executable use=CaptureBox,captures observable=value -->
```w
object CaptureBox {
  value: String
}

fn captures(
  copied: String,
  borrowed: ref String,
  moved: take String,
  sharedValue: shared CaptureBox,
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

fn classify(signal: Signal): String {
  return switch signal {
    case .quiet: "quiet"
    case .alert(let level) if level > 0: "alert"
    case .alert(_): "silent-alert"
  }
}

fn accumulate(values: Array<i32>): i32 {
  var total = 0
  rows: for value in values {
    guard value >= 0 else { continue rows }
    total += value
  }

  var attempts = 0
  while attempts < 2 { attempts += 1 }
  repeat { total += 1 } while total < 4

  return if total > 0 { total } else { 0 }
}

test "control flow returns an observable value" for accumulate {
  let signal: Signal = .alert(level: 1)
  expect classify(signal) == "alert"
  expect accumulate([1, -1, 2]) == 4
}
```

## Errors and cleanup

<!-- w-example role=executable use=ParseError,positive,parseAndClose,asyncCleanup observable=value -->
```w
enum ParseError: Error {
  negative
}

fn positive(value: i32): i32 throws ParseError {
  guard value >= 0 else { throw .negative }
  return value
}

fn parseAndClose(value: i32, closed: inout Bool): i32 {
  defer { closed = true }
  do {
    return try positive(value)
  } catch .negative {
    return 0
  }
}

async fn asyncCleanup(value: i32): i32 {
  defer async { await execution#yield() }
  return value
}

test "do/catch handles typed errors and defer closes" for parseAndClose {
  let error: ParseError = .negative
  var closed = false
  expect error == .negative
  expect parseAndClose(-1, closed: inout closed) == 0
  expect closed
  expect (try? positive(-1)) == .none
  expect await asyncCleanup(42) == 42
}
```

## Allocator scopes

<!-- w-example role=executable use=stage,prepare observable=value -->
```w
fn stage(allocator destination: ref Allocator, city: String): String {
  return city
}

fn prepare(city: String): (String, usize) {
  var result = city
  var bytes: usize = 0

  allocator scratch: .fixed<capacity: 256> {
    var copyOfCity = city
    let inout writable = copyOfCity
    writable = city
    result = stage(take copyOfCity)
  }

  allocator .fixed<capacity: 128> {
    bytes = result.bytes.count
  }

  return (result, bytes)
}

test "allocator scopes bound temporary work" for prepare {
  expect prepare("city") == ("city", 4)
}
```

## Unsafe, addresses, and bit operations

<!-- w-example role=executable use=clearTag observable=value -->
```w
unsafe fn clearTag(pointer: Address<.virtual, .readWrite>, tagMask: usize): usize {
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

async fn fetch(city: String): String throws FetchError {
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

async fn work(value: String): String throws WorkError { return value }

struct Trace {
  const requestId = TaskLocal<String?>.key(default: .none)
}

async fn traced(value: String): String throws WorkError {
  return try await Trace.requestId.withValue(
    .some("request-42"),
    operation: () => try await work(value + Trace.requestId.get()?),
  )
}

async fn cancelAndObserve(): TaskOutcome<String, WorkError> {
  let child = async work("cancelable")
  child#cancel(reason: .shutdown)
  return await (take child)#outcome()
}

async fn timed(value: String, timeout: TaskTimeout): TaskOutcome<String, WorkError> {
  return await Task.withTimeout(
    for: timeout,
    input: value,
    using: work,
  )
}

async fn first(): TaskSettlement<String, WorkError> {
  let primary = async work("primary")
  let fallback = spawn<.compute> work("fallback")
  let candidates: [Task<String, WorkError>; 2] = [primary, fallback]
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

async fn process(value: i32): i32 throws JobError { return value * 2 }

async fn processAll(values: take Array<i32>): Array<i32> throws JobError {
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
  values: take Array<i32>,
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
  temperature: u16
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
  let ready = await pipeline oven.acquire(220).preheat()
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

## Streams and channels

<!-- w-example role=executable use=StreamError,project,relay observable=value -->
```w
enum StreamError: Error { closed }

fn project(source: take Stream<String, Never>): some Stream<String, Never> {
  return stream <[take source]> {
    var cursor = take source
    while let item = await cursor.next() {
      yield copy item
    }
  }
}

async fn relay(
  source: take Stream<String, StreamError>,
  sender: Channel<String><.send>,
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
  let channel = Channel<String>.bounded(capacity: 2)
  let received = async channel.receiver.receive()
  let relayed = async relay(Stream.from(["east"]), channel.sender)
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
  revision: u64
  value: String
}

fn publish(ledger: shared Ledger, message: String): (usize, u64) {
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

## Execution context and process entry

<!-- w-example role=executable use=run observable=effect -->
```w
import process from std

async fn run(args: process.Arguments) {
  execution#checkCancellation()
  let clock = execution.clock()
  let started = clock.now()
  await execution#yield()
  print("args=${args.count}, elapsed=${clock.duration(from: started, to: clock.now())}")
}

entry(run)
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
  features: ref FeatureBatch<rows: rows, columns: inputs>,
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
  fn abs(value: c.int): c.int
}

export foreign c {
  struct w_result { value: c.int }
}

export unsafe fn<abi: .c> w_add(left: c.int, right: c.int): w_result {
  return w_result(value: left + right)
}

unsafe fn<lang: .c> c_add(left: c.int, right: c.int): c.int {
  return left + right;
}

unsafe fn callC(left: i32, right: i32): i32 {
  return abs(c_add(left, right))
}

test "foreign calls remain inside unsafe" for callC {
  expect unsafe { callC(20, 22) } == 42
  expect unsafe { w_add(20, 22).value } == 42
}
```

## Packages and workspaces

`build.w` is data, so it stays separate from module source:

<!-- w-example role=logical-contract -->
```w
// excerpt-kind: manifest-fragment
package: {
  name: "last-light"
  modules: ["app"]
  products: [{ name: "last-light-native", entry: "LastLightTui" }]
}

workspace: {
  members: ["packages/core", "packages/server"]
}
```

```text
w check
w run last-light-native -- --tui
```

The normative contract and implementation status remain in [DESIGN.md](DESIGN.md).
