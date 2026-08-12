// Native process entry values and capability projections.
//
// The module exports nominal wrappers. It does not create a global process
// object. The narrow `process.args`, `process.context`, `process.clock`, and
// `process.deadline` spellings are compiler projections available only inside a
// native-process entry root. `process.clock` preserves identity, origin,
// authority, and lifetime from `process.context.clock`. `process.deadline`
// preserves value identity, origin, and lifetime from `process.context.deadline`.
// Deadline is not authority, so the short projection does not expand it.
// Each short projection has the availability of its long projection.
// `ctx.clock` remains valid when Context is a parameter. No global lookup or
// `Clock.current` exists.

import * from std.io
import fs from std
import net from std
import time from std

export enum ExitCode: Copy & Equatable {
  success
  failure(u8<(1...)>)
}

export enum Signal: Copy & Equatable & Hashable {
  interrupt
  terminate
}

export enum SignalError: Error & Duplicable {
  denied
  unavailable
  unsupported(Signal)
  emptySet
  duplicate(Signal)
  closed
  limitExceeded(maximumRegistrations: usize)
}

export enum InputError: Error {
  io(IoError)
  invalidUtf8
  lineTooLong(maximumBytes: usize)
}

foreign intrinsic from "std.process@1" {
  type ArgumentsHandle
  type InputHandle
  type OutputHandle
  type LineStreamHandle
  type SignalRegistryHandle
  type SignalRegistrationHandle
  type ServicesHandle
  type ContextHandle

  fn stdProcessArgumentsCount(handle: ref ArgumentsHandle): usize
  fn stdProcessArgumentsGet(
    handle: ref ArgumentsHandle,
    index: usize,
  ): ref OsString?
  fn stdProcessArgumentsContainsNative(
    handle: ref ArgumentsHandle,
    value: ref OsString,
  ): Bool
  fn stdProcessArgumentsContainsText(
    handle: ref ArgumentsHandle,
    value: ref String,
  ): Bool
  fn stdProcessArgumentsDrop(handle: inout ArgumentsHandle)

  async fn stdProcessInputRead(
    handle: inout InputHandle,
    appendTo destination: inout Bytes,
    named maximum: usize<(1...)>,
  ): ReadStep throws IoError
  fn stdProcessInputLines(
    handle: ref InputHandle,
    maximumBytes: usize<(1...)>,
  ): LineStreamHandle
  fn stdProcessInputDrop(handle: inout InputHandle)
  async fn stdProcessLineNext(
    handle: inout LineStreamHandle,
  ): String? throws InputError
  async fn stdProcessLineCancel(
    handle: inout LineStreamHandle,
  ): () throws InputError
  fn stdProcessLineDrop(handle: inout LineStreamHandle)

  async fn stdProcessOutputWriteBytes(
    handle: inout OutputHandle,
    source: view Bytes,
  ): WriteStep throws IoError
  async fn stdProcessOutputWriteText(
    handle: ref OutputHandle,
    source: view String,
  ): () throws WriteAllError<IoError>
  fn stdProcessOutputDrop(handle: inout OutputHandle)

  fn stdProcessSignalRegister(
    handle: ref SignalRegistryHandle,
    signals: view Array<Signal>,
    handler: some async fn(Signal, Context): (),
  ): SignalRegistrationHandle throws SignalError
  fn stdProcessSignalReplace(
    handle: ref SignalRegistrationHandle,
    handler: some async fn(Signal, Context): (),
  ): () throws SignalError
  fn stdProcessSignalCancel(handle: ref SignalRegistrationHandle)
  fn stdProcessSignalRegistryDrop(handle: inout SignalRegistryHandle)
  fn stdProcessSignalRegistrationDrop(
    handle: inout SignalRegistrationHandle,
  )

  async fn stdProcessServicesDrain(
    handle: ref ServicesHandle,
    deadline: time.Deadline,
  )
  fn stdProcessServicesDrop(handle: inout ServicesHandle)

  fn stdProcessContextInput(handle: ref ContextHandle): InputHandle
  fn stdProcessContextOutput(handle: ref ContextHandle): OutputHandle
  fn stdProcessContextError(handle: ref ContextHandle): OutputHandle
  fn stdProcessContextFileSystem(handle: ref ContextHandle): fs.FileSystem
  fn stdProcessContextNetwork(handle: ref ContextHandle): net.Network
  fn stdProcessContextClock(handle: ref ContextHandle): time.Clock
  fn stdProcessContextSignals(handle: ref ContextHandle): SignalRegistryHandle
  fn stdProcessContextServices(handle: ref ContextHandle): ServicesHandle
  fn stdProcessContextDeadline(handle: ref ContextHandle): time.Deadline
  fn stdProcessContextDrop(handle: inout ContextHandle)
}

export struct Arguments {
  handle: ArgumentsHandle

  init(validatedHandle: ArgumentsHandle) {
    self.handle = validatedHandle
  }

  export count: usize {
    get => unsafe { stdProcessArgumentsCount(ref handle) }
  }

  export isEmpty: Bool {
    get => count == 0
  }

  export fn get(index: usize): ref OsString? {
    return unsafe { stdProcessArgumentsGet(ref handle, index) }
  }

  export fn contains(native value: ref OsString): Bool {
    return unsafe { stdProcessArgumentsContainsNative(ref handle, value) }
  }

  // This overload encodes valid W text to the host-native representation and
  // compares it exactly. A native argument that is not valid W text cannot
  // match this overload; callers can compare OsString instead.
  export fn contains(value: ref String): Bool {
    return unsafe { stdProcessArgumentsContainsText(ref handle, value) }
  }

  deinit {
    unsafe { stdProcessArgumentsDrop(inout handle) }
  }
}

struct ArgumentIterator {
  arguments: ref Arguments
  index: usize

  init(arguments: ref Arguments) {
    self.arguments = arguments
    self.index = 0
  }

  mut fn next(): ref OsString? {
    let value = arguments.get(index)
    if value != .none { index += 1 }
    return value
  }
}

extension Arguments: Sequence {
  alias Item = ref OsString

  export fn iterator(): some Iterator<ref OsString> {
    return ArgumentIterator(arguments: ref self)
  }
}

export struct Input: ByteSource<IoError> {
  handle: InputHandle

  init(validatedHandle: InputHandle) {
    self.handle = validatedHandle
  }

  export mut async fn read(
    appendTo destination: inout Bytes,
    named maximum: usize<(1...)>,
  ): ReadStep throws IoError {
    return unsafe {
      try await stdProcessInputRead(
        inout handle,
        appendTo: inout destination,
        maximum: maximum,
      )
    }
  }

  export fn lines(
    named maximumBytes: usize<(1...)>,
  ): some Stream<String, InputError> {
    let stream = unsafe {
      stdProcessInputLines(ref handle, maximumBytes)
    }
    return LineStream(validatedHandle: stream)
  }

  deinit {
    unsafe { stdProcessInputDrop(inout handle) }
  }
}

struct LineStream: Stream<String, InputError> {
  handle: LineStreamHandle

  init(validatedHandle: LineStreamHandle) {
    self.handle = validatedHandle
  }

  mut async fn next(): String? throws InputError {
    return unsafe { try await stdProcessLineNext(inout handle) }
  }

  take async fn cancel(): () throws InputError {
    unsafe { try await stdProcessLineCancel(inout handle) }
  }

  deinit {
    unsafe { stdProcessLineDrop(inout handle) }
  }
}

export struct Output: ByteSink<IoError> {
  handle: OutputHandle

  init(validatedHandle: OutputHandle) {
    self.handle = validatedHandle
  }

  export mut async fn write(
    source: view Bytes,
  ): WriteStep throws IoError {
    return unsafe {
      try await stdProcessOutputWriteBytes(inout handle, source)
    }
  }

  export async fn writeAll(
    text source: view String,
  ): () throws WriteAllError<IoError> {
    unsafe { try await stdProcessOutputWriteText(ref handle, source) }
  }

  deinit {
    unsafe { stdProcessOutputDrop(inout handle) }
  }
}

export struct SignalRegistry {
  handle: SignalRegistryHandle

  init(validatedHandle: SignalRegistryHandle) {
    self.handle = validatedHandle
  }

  export fn register(
    signals: view Array<Signal>,
    named handler: some async fn(Signal, Context): (),
  ): SignalRegistration throws SignalError {
    let registration = unsafe {
      try stdProcessSignalRegister(ref handle, signals, handler)
    }
    return SignalRegistration(validatedHandle: registration)
  }

  deinit {
    unsafe { stdProcessSignalRegistryDrop(inout handle) }
  }
}

export struct SignalRegistration {
  handle: SignalRegistrationHandle

  init(validatedHandle: SignalRegistrationHandle) {
    self.handle = validatedHandle
  }

  export fn replace(
    named handler: some async fn(Signal, Context): (),
  ): () throws SignalError {
    unsafe { try stdProcessSignalReplace(ref handle, handler) }
  }

  export fn cancel() {
    unsafe { stdProcessSignalCancel(ref handle) }
  }

  deinit {
    // Drop closes admission. A callback already accepted remains a structured
    // child of the entry root and drains there.
    unsafe { stdProcessSignalRegistrationDrop(inout handle) }
  }
}

export struct Services {
  handle: ServicesHandle

  init(validatedHandle: ServicesHandle) {
    self.handle = validatedHandle
  }

  export async fn drain(named deadline: time.Deadline) {
    unsafe { await stdProcessServicesDrain(ref handle, deadline) }
  }

  deinit {
    unsafe { stdProcessServicesDrop(inout handle) }
  }
}

export struct Context {
  handle: ContextHandle

  init(validatedHandle: ContextHandle) {
    self.handle = validatedHandle
  }

  export stdin: Input {
    get => Input(validatedHandle: unsafe {
      stdProcessContextInput(ref handle)
    })
  }

  export stdout: Output {
    get => Output(validatedHandle: unsafe {
      stdProcessContextOutput(ref handle)
    })
  }

  export stderr: Output {
    get => Output(validatedHandle: unsafe {
      stdProcessContextError(ref handle)
    })
  }

  export filesystem: fs.FileSystem {
    get => unsafe { stdProcessContextFileSystem(ref handle) }
  }

  export network: net.Network {
    get => unsafe { stdProcessContextNetwork(ref handle) }
  }

  export clock: time.Clock {
    get => unsafe { stdProcessContextClock(ref handle) }
  }

  export signals: SignalRegistry {
    get => SignalRegistry(validatedHandle: unsafe {
      stdProcessContextSignals(ref handle)
    })
  }

  export services: Services {
    get => Services(validatedHandle: unsafe {
      stdProcessContextServices(ref handle)
    })
  }

  export deadline: time.Deadline {
    get => unsafe { stdProcessContextDeadline(ref handle) }
  }

  deinit {
    unsafe { stdProcessContextDrop(inout handle) }
  }
}

test "exit failure codes exclude success" {
  let success = ExitCode.success
  let failure = ExitCode.failure(1)
  expect success != failure
}

test "portable process signals remain closed" {
  let interrupt = Signal.interrupt
  let terminate = Signal.terminate
  expect interrupt != terminate
}
