// Typed bindings and errors for hermetic build transforms.

export struct Input<Value> {
  package name: String

  export const init(name: String) {
    self.name = name
  }
}

export struct Output<Value> {
  package name: String

  export const init(name: String) {
    self.name = name
  }
}

export enum Error: Error {
  unknownInput(name: String)
  unknownOutput(name: String)
  incompatibleInput(name: String)
  inputLimit(name: String, maximumBytes: usize)
  outputLimit(name: String, maximumBytes: usize)
  missingOutput(name: String)
  codec(name: String)
  unavailable
}

test "input and output limits remain distinct build failures" {
  let input: Error = .inputLimit(name: "menu", maximumBytes: 64<KiB>)
  let output: Error = .outputLimit(name: "bytecode", maximumBytes: 1<MiB>)
  expect input != output
}
