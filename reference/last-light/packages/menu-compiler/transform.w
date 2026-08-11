// Hermetic build-transform entry for kitchen cards.

import build from std
import {
  MenuBytecode,
  MenuCompileError,
  compileMenu,
} from last_light.menu.compiler

const menuSource = build.Input<String>(name: "menu")
const menuBytecode = build.Output<Bytes>(name: "bytecode")

export enum MenuTransformError: Error {
  build(build.Error)
  compile(MenuCompileError)
}

async fn transform(ctx: build.Context): () throws MenuTransformError {
  let source = try await ctx.read(
    string: menuSource,
    maximumBytes: 64<KiB>,
  )
  let compiled = try compileMenu(source)
  let MenuBytecode(bytes, _) = take compiled
  try await ctx.write(bytes: menuBytecode, value: take bytes)
}

entry(transform)
