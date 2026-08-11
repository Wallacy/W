// TechEmpower-compatible workload profile. No route bypasses normal APIs.

import cache from std
import database from std
import http from std
import json from std
import random from std
import url from std

export struct BenchmarkMessage: json.Codable {
  message: String
}

export struct World: json.Codable {
  id: i32
  randomNumber: i32
}

export struct CachedWorld: json.Codable {
  id: i32
  randomNumber: i32
}

export struct Fortune: json.Codable {
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

const fortunesTemplate = http.TemplateBinding(
  name: "fortunes",
  limits: http.TemplateLimits(
    maximumOutputBytes: 1<MiB>,
    maximumValues: 501,
  ),
  version: 1,
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
  response(http.ResponseError)
  template(http.TemplateError)
  transaction(database.TransactionFailure<database.DatabaseError>)
}

enum QueryEditError: Error {
  rejected
}

fn boundedCount(
  parameters: ref url.URLSearchParams,
  parameter key: ref String,
): usize<(1...500)> {
  let count = parameters.get(key)
    .flatMap((value) => usize.parse(value))
    ?? 1

  return switch count {
    case ..<1: 1
    case 500...: 500
    case 1..<500: count
  }
}

fn boundedRequestCount(
  request: ref http.Request,
  parameter key: ref String,
): usize<(1...500)> {
  let parameters = request.url.searchParams()
  return boundedCount(parameters, parameter: key)
}

fn rejectQueryEdit(
  parameters: inout url.URLSearchParams,
): () throws QueryEditError {
  parameters.append("discarded", "1")
  throw .rejected
}

fn randomWorldKeys(
  count: usize<(1...500)>,
  ctx context: ref http.Context,
): Array<WorldKey> {
  var keys = Array<WorldKey>(minimumCapacity: count)

  for _ in 0..<count {
    keys.append(WorldKey(id: context.random.integer(in: 1...10_000)))
  }

  return keys
}

async fn world(ctx: http.Context): World throws BenchmarkError {
  let store = ctx.databases.get(benchmarkDatabase)
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
  ctx context: http.Context,
): Array<World> throws BenchmarkError {
  let store = context.databases.get(benchmarkDatabase)
  let keys = randomWorldKeys(count, ctx: context)
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
  let store = ctx.databases.get(benchmarkDatabase)
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

  let template = ctx.templates.get(fortunesTemplate)
  let page = try template.render(values: ref fortunes)
  var headers = http.Headers()
  try headers.set("content-type", "text/html; charset=utf-8")
  return try http.Response(take page, headers: take headers)
}

async fn updateWorlds(
  count: usize<(1...500)>,
  ctx context: http.Context,
): Array<World> throws BenchmarkError {
  let store = context.databases.get(benchmarkDatabase)
  let keys = randomWorldKeys(count, ctx: context)
  let rows = try await store.queryMany(
    worldById,
    parameters: take keys,
    maximumInFlight: 20,
  )
  var result = decodeWorlds(take rows)

  for inout world in result {
    world.randomNumber = context.random.integer(in: 1...10_000)
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
  _ store: ref any database.Database,
): CachedWorld throws database.DatabaseError {
  let row = try await store.one(
    cachedWorldById,
    parameters: WorldKey(id: id),
  )
  return CachedWorld(id: row.id, randomNumber: row.randomNumber)
}

async fn cachedQueries(
  count: usize<(1...500)>,
  ctx context: http.Context,
): Array<CachedWorld> throws BenchmarkError {
  let store = context.databases.get(benchmarkDatabase)
  let local = context.caches.get(cachedWorlds)
  let loader = capture(ref store) (id: ref i32) => {
    return try await loadCachedWorld(id, store: store)
  }
  var result = Array<CachedWorld>(minimumCapacity: count)

  for _ in 0..<count {
    let id = context.random.integer(in: 1...10_000)
    result.append(try await local.getOrLoad(id, using: loader))
  }

  return result
}

async fn fetchBenchmark(
  request: take http.Request,
  ctx: http.Context,
): http.Response throws BenchmarkError {
  return switch (request.method, request.url.pathname) {
    case (.get, "/plaintext"):
      try http.Response("Hello, World!")
    case (.get, "/json"):
      let payload = BenchmarkMessage(message: "Hello, World!")
      try http.Response.json(
        value: ref payload,
        maximumBytes: 64<KiB>,
      )
    case (.get, "/db"):
      let payload = try await world(ctx)
      try http.Response.json(value: ref payload, maximumBytes: 64<KiB>)
    case (.get, "/queries"):
      let count = boundedRequestCount(request, parameter: "queries")
      let payload = try await worlds(count, ctx: ctx)
      try http.Response.json(value: ref payload, maximumBytes: 64<KiB>)
    case (.get, "/fortunes"):
      try await renderFortunes(ctx)
    case (.get, "/updates"):
      let count = boundedRequestCount(request, parameter: "queries")
      let payload = try await updateWorlds(count, ctx: ctx)
      try http.Response.json(
        value: ref payload,
        maximumBytes: 64<KiB>,
      )
    case (.get, "/cached-queries"):
      let count = boundedRequestCount(request, parameter: "count")
      let payload = try await cachedQueries(count, ctx: ctx)
      try http.Response.json(
        value: ref payload,
        maximumBytes: 64<KiB>,
      )
    case (_, _):
      try http.Response(status: http.StatusCode.notFound)
  }
}

test "benchmark query URLs preserve canonical Web semantics" for boundedCount {
  let encoded = url.URLSearchParams(
    "queries=%35%30%30&queries=2&note=%2B+dessert",
  )
  expect boundedCount(encoded, parameter: "queries") == 500
  expect encoded.getAll("queries") == ["500", "2"]
  expect encoded.get("note") == "+ dessert"

  let base = try url.URL("https://faß.example:443/bench/round?#")
  var target = try url.URL("../queries?", base: base)
  expect target.href == "https://xn--fa-hia.example/queries?"

  target.editSearchParams((params) => {
    params.append("queries", "500")
    params.append("queries", "2")
    params.append("note", "+ dessert")
  })

  expect target.href
    == "https://xn--fa-hia.example/queries?queries=500&queries=2&note=%2B+dessert"
  let targetParameters = target.searchParams()
  expect boundedCount(targetParameters, parameter: "queries") == 500

  let beforeRejectedEdit = copy target.href
  do {
    try target.editSearchParams(rejectQueryEdit)
    panic("fallible URL edit was accepted")
  } catch .rejected {}
  expect target.href == beforeRejectedEdit

  var empty = try url.URL("https://example.test/queries?")
  empty.editSearchParams((_) => {})
  expect empty.href == "https://example.test/queries?"

  empty.editSearchParams((parameters) => { parameters.sort() })
  expect empty.href == "https://example.test/queries"

  empty.setSearch("?")
  expect empty.href == "https://example.test/queries?"
  empty.editSearchParams((parameters) => { parameters.delete("missing") })
  expect empty.href == "https://example.test/queries"
}

entry LastLightBenchmark(fetchBenchmark)
