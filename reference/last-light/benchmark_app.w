// TechEmpower-compatible workload profile. No route bypasses normal APIs.

import cache from std
import database from std
import http from std
import random from std

export struct BenchmarkMessage {
  message: String
}

export struct World {
  id: i32
  randomNumber: i32
}

export struct CachedWorld {
  id: i32
  randomNumber: i32
}

export struct Fortune {
  id: i32
  message: String
}

struct WorldKey {
  id: i32
}

struct WorldUpdate {
  id: i32
  randomNumber: i32
}

type WorldRow = (id: i32, randomNumber: i32)
type FortuneRow = (id: i32, message: String)

const benchmarkDatabase = database.Binding(
  name: "benchmark-database",
  dialect: .postgresql,
)

const cachedWorlds = cache.LocalBinding<i32, CachedWorld>(
  name: "cached-worlds",
  maximumEntries: 10_000,
  maximumActiveLoads: 256,
  maximumQueuedLoads: 4_096,
)

const worldById: database.Query<WorldKey, WorldRow> = database.Query(
  text: #"""
    SELECT id, randomNumber
    FROM World
    WHERE id = :id
    """#,
  dialect: .postgresql,
)

const cachedWorldById: database.Query<WorldKey, WorldRow> = database.Query(
  text: #"""
    SELECT id, randomNumber
    FROM CachedWorld
    WHERE id = :id
    """#,
  dialect: .postgresql,
)

const allFortunes: database.Query<(), FortuneRow> = database.Query(
  text: #"SELECT id, message FROM Fortune"#,
  dialect: .postgresql,
)

const updateWorld: database.Command<WorldUpdate> = database.Command(
  text: #"""
    UPDATE World
    SET randomNumber = :randomNumber
    WHERE id = :id
    """#,
  dialect: .postgresql,
)

export enum BenchmarkError: Error {
  cache(cache.CacheError)
  cacheLoad(cache.LoadFailure<database.DatabaseError>)
  database(database.DatabaseError)
  response(ResponseError)
  template(TemplateError)
  transaction(database.TransactionFailure<database.DatabaseError>)
}

fn boundedCount(
  request: ref http.Request,
  parameter: ref String,
): usize<(1...500)> {
  let count = request.query.get(parameter)
    .flatMap((value) => usize.parse(value))
    ?? 1

  return switch count {
    case ..<1: 1
    case 500...: 500
    case 1..<500: count
  }
}

fn randomWorldKeys(
  count: usize<(1...500)>,
  ctx: ref http.Context,
): Array<WorldKey> {
  var keys = Array<WorldKey>(minimumCapacity: count)

  for _ in 0..<count {
    keys.append(WorldKey(id: ctx.random.integer(in: 1...10_000)))
  }

  return keys
}

async fn world(ctx: http.Context): World throws BenchmarkError {
  let store = try ctx.databases.get(benchmarkDatabase)
  let row = try await store.one(
    worldById,
    parameters: WorldKey(id: ctx.random.integer(in: 1...10_000)),
  )
  return World(id: row.id, randomNumber: row.randomNumber)
}

fn decodeWorlds(rows: take Array<WorldRow>): Array<World> {
  var worlds = Array<World>(minimumCapacity: rows.count)

  for row in rows {
    worlds.append(World(id: row.id, randomNumber: row.randomNumber))
  }

  return worlds
}

async fn worlds(
  count: usize<(1...500)>,
  ctx: http.Context,
): Array<World> throws BenchmarkError {
  let store = try ctx.databases.get(benchmarkDatabase)
  let keys = randomWorldKeys(count, ctx: ctx)
  let rows = try await store.queryMany(
    worldById,
    parameters: take keys,
    maximumInFlight: 20,
  )
  return decodeWorlds(take rows)
}

async fn renderFortunes(
  ctx: http.Context,
): http.Response throws BenchmarkError {
  let store = try ctx.databases.get(benchmarkDatabase)
  let rows = try await store.all(
    allFortunes,
    parameters: (),
    limits: database.RowLimits(rows: 64, bytes: 64<KiB>),
  )
  var fortunes = Array<Fortune>(minimumCapacity: rows.count + 1)

  for row in rows {
    fortunes.append(Fortune(id: row.id, message: row.message))
  }

  fortunes.append(Fortune(
    id: 0,
    message: "Additional fortune added at request time.",
  ))
  fortunes.sort(by: (left, right) => left.message.compare(right.message))

  let page = try ctx.templates.render("fortunes", values: fortunes)
  return http.Response.html(page)
}

async fn updateWorlds(
  count: usize<(1...500)>,
  ctx: http.Context,
): Array<World> throws BenchmarkError {
  let store = try ctx.databases.get(benchmarkDatabase)
  let keys = randomWorldKeys(count, ctx: ctx)
  let rows = try await store.queryMany(
    worldById,
    parameters: take keys,
    maximumInFlight: 20,
  )
  var result = decodeWorlds(take rows)

  for inout world in result {
    world.randomNumber = ctx.random.integer(in: 1...10_000)
  }

  return try await transaction<
    isolation: .readCommitted,
    access: .readWrite,
  > tx = store {
    var updates = Array<WorldUpdate>(minimumCapacity: result.count)

    for world in result {
      updates.append(WorldUpdate(
        id: world.id,
        randomNumber: world.randomNumber,
      ))
    }

    let _ = try await tx.executeMany(updateWorld, parameters: take updates)
    commit take result
  }
}

async fn loadCachedWorld(
  id: ref i32,
  store: ref any database.Database,
): CachedWorld throws database.DatabaseError {
  let row = try await store.one(
    cachedWorldById,
    parameters: WorldKey(id: id),
  )
  return CachedWorld(id: row.id, randomNumber: row.randomNumber)
}

async fn cachedQueries(
  count: usize<(1...500)>,
  ctx: http.Context,
): Array<CachedWorld> throws BenchmarkError {
  let store = try ctx.databases.get(benchmarkDatabase)
  let local = try ctx.caches.get(cachedWorlds)
  let loader = capture(ref store) (id: ref i32) => {
    return try await loadCachedWorld(id, store: store)
  }
  var result = Array<CachedWorld>(minimumCapacity: count)

  for _ in 0..<count {
    let id = ctx.random.integer(in: 1...10_000)
    result.append(try await local.getOrLoad(id, using: loader))
  }

  return result
}

async fn fetchBenchmark(
  request: take http.Request,
  ctx: http.Context,
): http.Response throws BenchmarkError {
  return switch (request.method, request.path) {
    case (.get, "/plaintext"):
      http.Response.text("Hello, World!")
    case (.get, "/json"):
      try http.Response.json(BenchmarkMessage(message: "Hello, World!"))
    case (.get, "/db"):
      try http.Response.json(try await world(ctx))
    case (.get, "/queries"):
      let count = boundedCount(request, parameter: "queries")
      try http.Response.json(try await worlds(count, ctx: ctx))
    case (.get, "/fortunes"):
      try await renderFortunes(ctx)
    case (.get, "/updates"):
      let count = boundedCount(request, parameter: "queries")
      try http.Response.json(try await updateWorlds(count, ctx: ctx))
    case (.get, "/cached-queries"):
      let count = boundedCount(request, parameter: "count")
      try http.Response.json(try await cachedQueries(count, ctx: ctx))
    case (_, _):
      http.Response(status: .notFound)
  }
}

entry LastLightBenchmark(fetchBenchmark)
