// Host bindings for CLI and HTTP.

import std.http
import { Command, CommandError, decodeCommand } from restaurant.command
import { Order } from restaurant.domain
import { RestaurantApi, RestaurantError } from restaurant.restaurant
import { commandLimit } from restaurant.units

export enum AppError: Error {
  command(CommandError)
  decode(DecodeError)
  restaurant(RestaurantError)
  response(ResponseError)
}

async fn dispatch(command: take Command, restaurant: ServiceRef<RestaurantApi>): String throws AppError {
  return switch command {
    case .place(let order):
      let receipt = try await restaurant.place(take order)
      "Comanda ${receipt.orderId}: ${receipt.total}"
    case .status(let orderId):
      let stage = try await restaurant.status(orderId)
      "Comanda ${orderId}: ${stage}"
    case .cancel(let orderId):
      let stage = try await restaurant.cancel(orderId)
      "Comanda ${orderId}: ${stage}"
    case .shutdown:
      "Encerramento solicitado."
  }
}

async fn run(args: ProcessArguments, ctx: ProcessContext): ExitCode throws AppError {
  let restaurant = try await ctx.services.get<RestaurantApi>(key: "last-light")
  print("Restaurante Última Luz pronto.")

  for line in ctx.stdin.lines(limit: commandLimit) {
    let command = try decodeCommand(line)
    print(try await dispatch(take command, restaurant: restaurant))
  }

  return .success
}

async fn readCommand(line: String, ctx: CliContext): Void throws AppError {
  let restaurant = try await ctx.services.get<RestaurantApi>(key: "last-light")
  let command = try decodeCommand(line)
  let output = try await dispatch(take command, restaurant: restaurant)
  try await ctx.stdout.write("${output}\n")
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
