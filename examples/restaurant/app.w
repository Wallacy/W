// Host bindings for CLI and HTTP.

import std.http
import { Order } from restaurant.domain
import { RestaurantApi, RestaurantError } from restaurant.restaurant
import { commandLimit } from restaurant.units

export enum AppError: Error {
  decode(DecodeError)
  restaurant(RestaurantError)
  response(ResponseError)
}

async fn run(args: ProcessArguments, ctx: ProcessContext): ExitCode throws AppError {
  let restaurant = try await ctx.services.get<RestaurantApi>(key: "last-light")
  print("Restaurante Última Luz pronto.")

  for line in ctx.stdin.lines(limit: commandLimit) {
    let order = try OrderCommand.decode(line).order()
    let receipt = try await restaurant.place(take order)
    print("Comanda ${receipt.orderId}: ${receipt.total}")
  }

  return .success
}

async fn readCommand(line: String, ctx: CliContext): Void throws AppError {
  let restaurant = try await ctx.services.get<RestaurantApi>(key: "last-light")
  let order = try OrderCommand.decode(line).order()
  let receipt = try await restaurant.place(take order)
  try await ctx.stdout.write("${receipt}\n")
}

async fn fetch(request: http.Request, ctx: http.Context): http.Response throws AppError {
  let restaurant = try await ctx.services.get<RestaurantApi>(key: "last-light")
  let order = try request.json.decode<Order>()
  let receipt = try await restaurant.place(take order)
  return try http.Response.json(receipt)
}

async fn shutdown(signal: ProcessSignal, ctx: ProcessContext): Void {
  print("Encerrando o último turno por ${signal}.")
  await ctx.services.drain(deadline: ctx.deadline)
}

entry LastLight {
  process.main = run
  process.stdinLine = readCommand
  process.signal = shutdown
  http.fetch = fetch
}
