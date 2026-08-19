// atlas:begin module-run-root
module atlas_execution
import std.runtime.task
import { Stream, Channel } from std.stream
// atlas:end module-run-root

// atlas:begin allocator-and-bindings
fn stage(allocator destination: ref Allocator, city: String): String {
  return city
}

fn prepare(city: String): String {
  var result = city
  allocator scratch: .fixed<capacity: 256> {
    let ref name = city
    var copyOfName = city
    let inout writableName = copyOfName
    var atomic count: usize = 0
    count += 1
    let moved = take writableName
    result = moved
    let staged = stage(city)
    let _ = staged
  }
  allocator .fixed<capacity: 128> {
    let _ = result.bytes.count
  }
  allocator .root {
    let rootName = result.bytes.count
    let _ = rootName
  }
  try allocator .none {
    let _ = result.bytes.count
  }
  return result
}
// atlas:end allocator-and-bindings

// atlas:begin control-flow
fn walk(values: Array<i32>): i32 throws String {
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
    if total > 8 { break }
  } catch {
    total = 0
  }
  guard total >= 0 else { throw "negative" }
  defer { total += 1 }
  return total
}
// atlas:end control-flow

// atlas:begin execution-forms
async fn fetch(city: String): String throws String {
  return city
}

async fn runTasks(): String throws String {
  async let direct = fetch("north")
  spawn<.compute> let parallel = fetch("south")
  let first = try await direct
  let second = try await parallel
  let optional = try? await fetch("west")
  return first + second + optional?
}

fn inspect<T>(each values: T...): String {
  return "inspected"
}

fn directCall(values: Array<String>): String {
  let head = values[0]?.trim()?.value?
  return inspect(each values)
}

fn captureModes(target: String, borrowed: ref String, moved: take String, sharedValue: shared String): String {
  let copyCapture = <[copy target]>() => target
  let refCapture = <[ref borrowed]>() => borrowed
  let takeCapture = <[take moved]>() => moved
  let weakCapture = <[weak sharedValue]>() => sharedValue
  let _ = copyCapture()
  let _ = refCapture()
  let _ = takeCapture()
  let _ = weakCapture()
  return target
}
// atlas:end execution-forms

// atlas:begin restricted-expressions
struct AtlasLease {
  target: String
}

fn acquireLease(target: String): AtlasLease {
  return AtlasLease(target: target)
}

fn prepareLease(lease: AtlasLease): String {
  return lease.target
}

async fn restricted(target: String): String throws String {
  let captured = <[copy target]>(name) => name
  let value = if target == "north" { "day" } else { "night" }
  let range = 1..<4
  let (lease, ready) = try await pipeline {
    let lease = acquireLease(target)
    let ready = prepareLease(lease)
    return (lease, ready)
  }
  let guarded = lock target as city {
    city
  }
  let transactionValue = transaction<.serial> tx = target {
    commit tx
  }
  let unsafeValue = unsafe {
    target
  }
  let pinned = pin target
  let _ = captured
  let _ = range
  let _ = lease
  let _ = ready
  let _ = guarded
  let _ = transactionValue
  let _ = unsafeValue
  let _ = pinned
  return target
}

fn panicExample(message: String): String {
  panic(message: message)
}
// atlas:end restricted-expressions

// atlas:begin stream-and-channel
async fn consume(source: Stream<view String, String>, channel: Channel<String><.receive>): String throws String {
  var result = ""
  for try await ref item in source {
    result = result + item
  }
  await channel.close()
  return result
}

async fn send(channel: Channel<String><.send>, value: String): String throws String {
  await channel.send(take value)
  return "sent"
}

fn project(source: take Stream<String, Never>): some Stream<String, Never> {
  return stream <[take source]> {
    var cursor = take source
    while let item = await cursor.next() {
      yield copy item
    }
  }
}
// atlas:end stream-and-channel

fn print(value: String) {
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
