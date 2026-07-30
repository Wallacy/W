// Hermetic build-transform entry for kitchen cards.

import std.build
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
    menuSource,
    maximumBytes: 64<KiB>,
  )
  let compiled = try compileMenu(source)
  let MenuBytecode(bytes, _) = take compiled
  try await ctx.write(menuBytecode, take bytes)
}

entry(transform)
