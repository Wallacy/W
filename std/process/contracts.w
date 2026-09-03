// Native process entry values and capability projections.
//
// The module exports nominal wrappers. It does not create a global process
// object or contextual namespace. `execution` is the target-neutral contextual
// root. Process Arguments and Context enter reusable code only as explicit
// parameters. `ctx.clock()` remains valid when Context is a parameter. The default call is
// nonthrowing when the capability is available; active policy selection is
// fallible and accepts only included/excluded. No global lookup or
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

  fn stdProcessArgumentsCount(_ handle: ref ArgumentsHandle): usize
  fn stdProcessArgumentsGet(
    _ handle: ref ArgumentsHandle,
    _ index: usize,
  ): ref OsString?
  fn stdProcessArgumentsContainsNative(
    _ handle: ref ArgumentsHandle,
    _ value: ref OsString,
  ): Bool
  fn stdProcessArgumentsContainsText(
    _ handle: ref ArgumentsHandle,
    _ value: ref String,
  ): Bool
  fn stdProcessArgumentsDrop(_ handle: inout ArgumentsHandle)

  async fn stdProcessInputRead(
    _ handle: inout InputHandle,
    _ destination: inout Bytes,
    _ maximum: usize<(1...)>,
  ): ReadStep throws IoError
  fn stdProcessInputLines(
    _ handle: ref InputHandle,
    _ maximumBytes: usize<(1...)>,
  ): LineStreamHandle
  fn stdProcessInputDrop(_ handle: inout InputHandle)
  async fn stdProcessLineNext(
    _ handle: inout LineStreamHandle,
  ): String? throws InputError
  async fn stdProcessLineCancel(
    _ handle: inout LineStreamHandle,
  ): () throws InputError
  fn stdProcessLineDrop(_ handle: inout LineStreamHandle)

  async fn stdProcessOutputWriteBytes(
    _ handle: inout OutputHandle,
    _ source: view Bytes,
  ): WriteStep throws IoError
  async fn stdProcessOutputWriteText(
    _ handle: ref OutputHandle,
    _ source: view String,
  ): () throws WriteAllError<IoError>
  fn stdProcessOutputDrop(_ handle: inout OutputHandle)

  fn stdProcessSignalRegister(
    _ handle: ref SignalRegistryHandle,
    _ signals: view Array<Signal>,
    _ handler: some async fn(Signal, Context): (),
  ): SignalRegistrationHandle throws SignalError
  fn stdProcessSignalReplace(
    _ handle: ref SignalRegistrationHandle,
    _ handler: some async fn(Signal, Context): (),
  ): () throws SignalError
  fn stdProcessSignalCancel(_ handle: ref SignalRegistrationHandle)
  fn stdProcessSignalRegistryDrop(_ handle: inout SignalRegistryHandle)
  fn stdProcessSignalRegistrationDrop(
    _ handle: inout SignalRegistrationHandle,
  )

  async fn stdProcessServicesDrain(
    _ handle: ref ServicesHandle,
    _ deadline: time.Deadline,
  )
  fn stdProcessServicesDrop(_ handle: inout ServicesHandle)

  fn stdProcessContextInput(_ handle: ref ContextHandle): InputHandle
  fn stdProcessContextOutput(_ handle: ref ContextHandle): OutputHandle
  fn stdProcessContextError(_ handle: ref ContextHandle): OutputHandle
  fn stdProcessContextFileSystem(_ handle: ref ContextHandle): fs.FileSystem
  fn stdProcessContextNetwork(_ handle: ref ContextHandle): net.Network
  fn stdProcessContextClock(
    _ handle: ref ContextHandle,
  ): time.Clock
  fn stdProcessContextClockWithPolicy(
    _ handle: ref ContextHandle,
    _ policy: time.HostSuspendPolicy<[.included, .excluded]>,
  ): time.Clock throws time.ClockSelectionError
  fn stdProcessContextSignals(_ handle: ref ContextHandle): SignalRegistryHandle
  fn stdProcessContextServices(_ handle: ref ContextHandle): ServicesHandle
  fn stdProcessContextDeadline(_ handle: ref ContextHandle): time.Deadline
  fn stdProcessContextDrop(_ handle: inout ContextHandle)
}

export struct Arguments {
  let handle: ArgumentsHandle

  init(validatedHandle: ArgumentsHandle) {
    self.handle = validatedHandle
  }

  export let count: usize {
    get => unsafe { stdProcessArgumentsCount(ref handle) }
  }

  export let isEmpty: Bool {
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
  let arguments: ref Arguments
  let index: usize

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
  let handle: InputHandle

  init(validatedHandle: InputHandle) {
    self.handle = validatedHandle
  }

  export mut async fn read(
    appendTo destination: inout Bytes,
    maximum: usize<(1...)>,
  ): ReadStep throws IoError {
    return unsafe {
      try await stdProcessInputRead(
        inout handle,
        inout destination,
        maximum,
      )
    }
  }

  export fn lines(
    maximumBytes: usize<(1...)>,
  ): some Stream<String, InputError> {
    let rawStream = unsafe {
      stdProcessInputLines(ref handle, maximumBytes)
    }
    return LineStream(validatedHandle: rawStream)
  }

  deinit {
    unsafe { stdProcessInputDrop(inout handle) }
  }
}

struct LineStream: Stream<String, InputError> {
  let handle: LineStreamHandle

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
  let handle: OutputHandle

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
  let handle: SignalRegistryHandle

  init(validatedHandle: SignalRegistryHandle) {
    self.handle = validatedHandle
  }

  export fn register(
    signals: view Array<Signal>,
    handler: some async fn(Signal, Context): (),
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
  let handle: SignalRegistrationHandle

  init(validatedHandle: SignalRegistrationHandle) {
    self.handle = validatedHandle
  }

  export fn replace(
    handler: some async fn(Signal, Context): (),
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
  let handle: ServicesHandle

  init(validatedHandle: ServicesHandle) {
    self.handle = validatedHandle
  }

  export async fn drain(deadline: time.Deadline) {
    unsafe { await stdProcessServicesDrain(ref handle, deadline) }
  }

  deinit {
    unsafe { stdProcessServicesDrop(inout handle) }
  }
}

export struct Context {
  let handle: ContextHandle

  init(validatedHandle: ContextHandle) {
    self.handle = validatedHandle
  }

  export let stdin: Input {
    get => Input(validatedHandle: unsafe {
      stdProcessContextInput(ref handle)
    })
  }

  export let stdout: Output {
    get => Output(validatedHandle: unsafe {
      stdProcessContextOutput(ref handle)
    })
  }

  export let stderr: Output {
    get => Output(validatedHandle: unsafe {
      stdProcessContextError(ref handle)
    })
  }

  export let filesystem: fs.FileSystem {
    get => unsafe { stdProcessContextFileSystem(ref handle) }
  }

  export let network: net.Network {
    get => unsafe { stdProcessContextNetwork(ref handle) }
  }

  export fn clock(): time.Clock {
    return unsafe { stdProcessContextClock(ref handle) }
  }

  export fn clock(
    hostSuspend policy: time.HostSuspendPolicy<[.included, .excluded]>,
  ): time.Clock throws time.ClockSelectionError {
    return unsafe {
      try stdProcessContextClockWithPolicy(ref handle, policy)
    }
  }

  export let signals: SignalRegistry {
    get => SignalRegistry(validatedHandle: unsafe {
      stdProcessContextSignals(ref handle)
    })
  }

  export let services: Services {
    get => Services(validatedHandle: unsafe {
      stdProcessContextServices(ref handle)
    })
  }

  export let deadline: time.Deadline {
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
