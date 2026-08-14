import build from std

let menuSource = build.Input<String>(name: "menu")
let menuBytecode = build.Output<Bytes>(name: "bytecode")

async fn transform(ctx: build.Context): () throws build.Error {
  let source = try await ctx.read(string: menuSource, maximumBytes: 64KiB)
  try await ctx.write(bytes: menuBytecode, value: source.bytes)
}

entry(transform)
