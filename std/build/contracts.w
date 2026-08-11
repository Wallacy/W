// Typed bindings and errors for hermetic build transforms.

const fn isBindingName(value: ref String): Bool {
  if value.bytes.count < 1 || value.bytes.count > 64 { return false }

  var index: usize = 0
  for byte in value.bytes {
    let lowercase = byte >= b'a' && byte <= b'z'
    let digit = byte >= b'0' && byte <= b'9'
    let separator = byte == b'-'

    if index == 0 && !lowercase { return false }
    if index > 0 && !(lowercase || digit || separator) { return false }
    index += 1
  }

  return true
}

export struct Input<Value> {
  // Value is phantom. It selects the closed provider codec at the call site.
  name: String

  export const init(name: String) {
    assert(isBindingName(name), "invalid build input binding")
    self.name = name
  }
}

export struct Output<Value> {
  // Value is phantom. It selects the closed provider codec at the call site.
  name: String

  export const init(name: String) {
    assert(isBindingName(name), "invalid build output binding")
    self.name = name
  }
}

// Error conforms to Equatable because typed build failures are compared by
// the design oracle and by host boundary tests.
export enum Error: Error & Equatable {
  unknownInput(name: String)
  unknownOutput(name: String)
  incompatibleInput(name: String)
  incompatibleOutput(name: String)
  inputLimit(name: String, maximumBytes: usize)
  outputLimit(name: String, maximumBytes: usize)
  duplicateOutput(name: String)
  missingOutput(name: String)
  codec(name: String)
  unavailable
}

// The provider owns the action handle and its private staging area. Public W
// code sees only the typed descriptors and this nominal Context wrapper.
// The handle never crosses a service, wire, storage, or foreign boundary.
// Provider calls borrow the handle so shared Context borrows can run in
// parallel. Safe W reaches deinit only after every async borrow completes or
// passes cancellation drain. The synchronous drop releases residual state.
foreign intrinsic from "std.build@1" {
  type ContextHandle

  async fn stdBuildReadString(
    named handle: ref ContextHandle,
    named input: const Input<String>,
    named maximumBytes: usize<(1...)>,
  ): String throws Error
  async fn stdBuildReadBytes(
    named handle: ref ContextHandle,
    named input: const Input<Bytes>,
    named maximumBytes: usize<(1...)>,
  ): Bytes throws Error
  async fn stdBuildWriteString(
    named handle: ref ContextHandle,
    named output: const Output<String>,
    named value: take String,
  ): () throws Error
  async fn stdBuildWriteBytes(
    named handle: ref ContextHandle,
    named output: const Output<Bytes>,
    named value: take Bytes,
  ): () throws Error
  fn stdBuildContextDrop(handle: inout ContextHandle)
}

// Bytes use identity encoding. String uses strict UTF-8. The provider checks
// the effective declared byte bound before allocation or decode and does not
// normalize Unicode, newline, BOM, or path text. Only read(Input<String>) can
// raise .codec(name) for invalid input bytes. W String is already valid UTF-8,
// and SDK0 writes do not raise .codec. After preflight, the smallest applicable
// declared bound wins.

// Context is a move-only, process-local owner supplied by the
// w.host/build-transform@1 entry slot. It has no public initializer. It only
// reads inputs and materializes private output candidates.
export struct Context {
  handle: ContextHandle

  init(validatedHandle: ContextHandle) {
    self.handle = validatedHandle
  }

  // Shared reads return new owners. The effective bound is the smallest
  // declared call, action, and host-profile/toolchain-plan bound. Time and
  // space are linear in encoded bytes with bounded overhead.
  export async fn read(
    string input: const Input<String>,
    maximumBytes limit: usize<(1...)>,
  ): String throws Error {
    return unsafe {
      try await stdBuildReadString(
        handle: ref handle,
        input: input,
        maximumBytes: limit,
      )
    }
  }

  export async fn read(
    bytes input: const Input<Bytes>,
    maximumBytes limit: usize<(1...)>,
  ): Bytes throws Error {
    return unsafe {
      try await stdBuildReadBytes(
        handle: ref handle,
        input: input,
        maximumBytes: limit,
      )
    }
  }

  // write consumes value on success, build.Error, or task cancellation. The
  // provider prepares one candidate in private staging. A second write for
  // one binding invalidates the action, including unsafe or foreign calls.
  export async fn write(
    string output: const Output<String>,
    value content: take String,
  ): () throws Error {
    unsafe {
      try await stdBuildWriteString(
        handle: ref handle,
        output: output,
        value: take content,
      )
    }
  }

  export async fn write(
    bytes output: const Output<Bytes>,
    value content: take Bytes,
  ): () throws Error {
    unsafe {
      try await stdBuildWriteBytes(
        handle: ref handle,
        output: output,
        value: take content,
      )
    }
  }

  deinit {
    // Safe W proves that async borrows completed or passed cancellation drain
    // before this synchronous deinit. Drop releases the wrapper and residual
    // handle exactly once. It does not wait or drain.
    unsafe { stdBuildContextDrop(inout handle) }
  }
}

test "input and output limits remain distinct build failures" {
  let input: Error = .inputLimit(name: "menu", maximumBytes: 64<KiB>)
  let output: Error = .outputLimit(name: "bytecode", maximumBytes: 1<MiB>)
  expect input != output
}

test "build bindings use a closed portable alphabet" {
  let source = Input<String>(name: "menu-1")
  let output = Output<Bytes>(name: "menu-bytecode")

  expect source.name == "menu-1"
  expect output.name == "menu-bytecode"
}
