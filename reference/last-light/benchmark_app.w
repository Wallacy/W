// TechEmpower-compatible workload profile. No route may bypass normal APIs.

import std.database
import std.http
import std.random

export struct BenchmarkMessage {
  message: String
}

export struct World {
  id: i32
  randomNumber: i32
}

export struct Fortune {
  id: i32
  message: String
}

export enum BenchmarkError: Error {
  database(DatabaseError)
  decode(DecodeError)
  response(ResponseError)
}

fn queryCount(request: ref http.Request): usize<(1...500)> {
  let count = request.query.get("queries")
    .flatMap((value) => usize.parse(value))
    ?? 1

  return switch count {
    case ..<1: 1
    case 500...: 500
    case 1..<500: count
  }
}

async fn worlds(
  count: usize<(1...500)>,
  ctx: http.Context,
): Array<World> throws BenchmarkError {
  var result = Array<World>(minimumCapacity: count)

  for _ in 0..<count {
    let id = ctx.random.integer(in: 1...10_000)
    result.append(try await ctx.database.queryOne<World>(
      "SELECT id, randomNumber FROM World WHERE id = ?",
      arguments: [id],
    ))
  }

  return result
}

async fn fetchBenchmark(
  request: http.Request,
  ctx: http.Context,
): http.Response throws BenchmarkError {
  return switch request.path {
    case "/plaintext":
      http.Response.text("Hello, World!")
    case "/json":
      try http.Response.json(BenchmarkMessage(message: "Hello, World!"))
    case "/db":
      try http.Response.json(try await worlds(1, ctx: ctx).first)
    case "/queries":
      try http.Response.json(try await worlds(queryCount(request), ctx: ctx))
    case "/fortunes":
      let fortunes = try await ctx.database.query<Fortune>(
        "SELECT id, message FROM Fortune",
      )
      try http.Response.html(ctx.templates.render("fortunes", values: fortunes))
    case _:
      http.Response(status: .notFound)
  }
}

entry LastLightBenchmark(fetchBenchmark)
