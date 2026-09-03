// atlas:begin module-run-root
module atlas_execution
import std.runtime.task
import { Stream, Channel } from std.stream
// atlas:end module-run-root

enum AtlasError: Error {
  failed
}

// atlas:begin allocator-and-bindings
fn stage(city: String, allocator destination: ref Allocator): String {
  return city
}

fn rewrite(value: inout String) {
  value = value + "!"
}

fn prepare(_ city: String): (String, usize) {
  var result = city
  var byteCount: usize = 0
  allocator scratch: .fixed<capacity: 256> {
    let ref name = city
    var copyOfName = city
    let mut ref writableName = copyOfName
    rewrite(value: inout copyOfName)
    var atomic count: usize = 0
    count += 1
    writableName = name
    let moved = take copyOfName
    result = moved
    let staged = stage(city)
    let stagedExplicit = stage(city, allocator: ref scratch)
    result = staged
    result = stagedExplicit
  }
  allocator .fixed<capacity: 128> {
    byteCount = result.bytes.count
  }
  return (result, byteCount)
}
// atlas:end allocator-and-bindings

// atlas:begin control-flow
enum WalkError: Error {
  negativeTotal
}

fn requireNonnegative(_ value: i32): i32 throws WalkError {
  guard value >= 0 else { throw .negativeTotal }
  return value
}

fn walk(_ values: Array<i32>): i32 throws WalkError {
  var total = 0
  rows: for ref value in values {
    for column in [value] {
      if column < 0 {
        continue rows
      } else {
        total += column
      }
    }
  }
  var index = 0
  while index < 3 {
    index += 1
  }
  repeat {
    total += 1
  } while total < 4
  do {
    total = try requireNonnegative(total)
  } catch .negativeTotal {
    total = 0
  }
  defer { total += 1 }
  return total
}
// atlas:end control-flow

// atlas:begin execution-forms
async fn fetch(_ city: String): String throws AtlasError {
  return city
}

async fn runTasks(): String throws AtlasError {
  execution#checkCancellation()
  let clock = execution.clock()
  let started = clock.now()
  let direct = try sync fetch("north")
  let concurrent = async fetch("east")
  let parallel = spawn<.compute> fetch("south")
  let first = try await concurrent
  let second = try await parallel
  await execution#yield()
  let optional = try? await fetch("west")
  print("elapsed: ${clock.duration(from: started, to: clock.now())}")
  return first + second + direct + optional?
}

fn inspect<T>(each values: T...): String {
  return "inspected"
}

fn directCall(_ values: Array<String>): String {
  let head = values[0]?.trim()?.value?
  return inspect(each values)
}

object CaptureBox {
  value: String
}

fn captureModes(_ target: String, _ borrowed: ref String, _ moved: take String, _ sharedValue: shared CaptureBox): (String, String, String, String, String?) {
  let copyCapture = <[copy target]>() => target
  let refCapture = <[ref borrowed]>() => borrowed
  let takeCapture = <[take moved]>() => moved
  let weakCapture = <[weak sharedValue]>() => if let owner = sharedValue {
    .some(copy owner.value)
  } else {
    .none
  }
  let copied = copyCapture()
  let referenced = refCapture()
  let taken = takeCapture()
  let weakened = weakCapture()
  return (target, copied, referenced, taken, weakened)
}
// atlas:end execution-forms

// atlas:begin restricted-expressions
struct AtlasLease {
  target: String
}

fn acquireLease(_ target: String): AtlasLease {
  return AtlasLease(target: target)
}

fn prepareLease(_ lease: AtlasLease): String {
  return lease.target
}

async fn restricted(_ target: String): String throws AtlasError {
  let captured = <[copy target]>(name) => name
  let value = if target == "north" { "day" } else { "night" }
  let range = 1..<4
  let (lease, ready) = try await pipeline {
    let lease = ovens.acquire(target)
    let ready = lease.preheat()
    commit (lease, ready)
  }
  let chained = try await pipeline ovens.acquire(target).preheat()
  let outputs = try await pipeline<
    tasks: .concurrent,
    limit: 16,
    ordering: .input,
    errors: .failFast,
  > each item in take items {
    commit inspect(item)
  }
  let guarded = lock target as city {
    city
  }
  let transactionValue = try await pipeline<transaction: {
    isolation: .serializable,
    access: .readWrite,
  }> tx = store {
    commit tx.read()
  }
  let logical = target#label
  let observed = (target#version).mutationEpoch
  let unsafeValue = unsafe {
    target
  }
  let pinned = pin target
  let _ = captured
  let _ = range
  let _ = lease
  let _ = ready
  let _ = chained
  let _ = outputs
  let _ = guarded
  let _ = transactionValue
  let _ = logical
  let _ = observed
  let _ = unsafeValue
  let _ = pinned
  return target
}

fn panicExample(_ message: String): String {
  panic(message: message)
}
// atlas:end restricted-expressions

// atlas:begin stream-and-channel
async fn consume(_ source: Stream<view String, AtlasError>, _ channel: Channel<receive: String>): String throws AtlasError {
  var result = ""
  for try await ref item in source {
    result = result + item
  }
  await channel.close()
  return result
}

async fn send(_ channel: Channel<send: String>, _ value: String): String throws AtlasError {
  await channel.send(take value)
  return "sent"
}

fn project(_ source: take Stream<String, Never>): some Stream<String, Never> {
  return stream <[take source]> {
    var cursor = take source
    while let item = await cursor.next() {
      yield copy item
    }
  }
}
// atlas:end stream-and-channel

fn print(_ value: String) {
  let _ = value
}

// atlas:begin module-run-entry
fn runModuleRun() {
  let city = "north"
  let greeting = prepare(city)
  print(greeting)
}

entry(runModuleRun)
// atlas:end module-run-entry
