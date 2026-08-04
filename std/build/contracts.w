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
  name: String

  export const init(name: String) {
    assert(isBindingName(name), "invalid build input binding")
    self.name = name
  }
}

export struct Output<Value> {
  name: String

  export const init(name: String) {
    assert(isBindingName(name), "invalid build output binding")
    self.name = name
  }
}

export enum Error: Error {
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
