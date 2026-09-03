// Native thread-local storage for narrow system and adapter use.
//
// A task can migrate after suspension. ThreadLocal therefore does not model
// task context, an execution domain, or an isolation boundary.

foreign intrinsic from "std.runtime.thread-local@1" {
  type ThreadLocalIdentity

  const fn stdThreadLocalKey<Value: Copy>(_ initial: Value): ThreadLocalIdentity

  fn stdThreadLocalRead<Value: Copy>(
    _ identity: ref ThreadLocalIdentity,
  ): Value

  fn stdThreadLocalWrite<Value: Copy, Result, Failure: Error>(
    _ identity: ref ThreadLocalIdentity,
    _ operation: some fn(inout Value): Result throws Failure,
  ): Result throws Failure
}

export struct ThreadLocal<Value: Copy> {
  identity: ThreadLocalIdentity

  init(validatedIdentity: ThreadLocalIdentity) {
    self.identity = validatedIdentity
  }

  export static const fn key(initial: Value): ThreadLocal<Value> {
    let identity = unsafe { stdThreadLocalKey(initial) }
    return ThreadLocal<Value>(validatedIdentity: identity)
  }

  export fn read(): Value {
    return unsafe { stdThreadLocalRead(identity) }
  }

  export fn write<Result, Failure: Error>(
    operation: some fn(inout Value): Result throws Failure,
  ): Result throws Failure {
    return unsafe {
      try stdThreadLocalWrite(
        identity,
        operation,
      )
    }
  }
}

struct ThreadLocalTestCounters {
  const samples = ThreadLocal<u64>.key(initial: 0)
}

test "native threads keep independent slots" {
  let first = ThreadLocalTestCounters.samples.read()
  let second = ThreadLocalTestCounters.samples.write((value: inout u64) => {
    value += 1
    return value
  })

  expect first == 0
  expect second == 1
}
