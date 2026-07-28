// W Working Draft — pseudocódigo pedagógico, não executável.
// Duas interfaces long-lived concorrem; nenhuma cria thread por ser async.

import { RestaurantApi } from restaurant.front_desk
import { TerminalError, runTerminal } from restaurant.terminal
import { WebError, serveWeb } from restaurant.web

export enum AppError: Error {
  terminal(TerminalError)
  web(WebError)
}

async fn runTerminalInterface(restaurant: ServiceRef<RestaurantApi>): Void throws AppError {
  do {
    return try await runTerminal(restaurant)
  } catch let error {
    throw .terminal(error)
  }
}

async fn runWebInterface(address: http.Address, restaurant: ServiceRef<RestaurantApi>): Void throws AppError {
  do {
    return try await serveWeb(address, restaurant: restaurant)
  } catch let error {
    throw .web(error)
  }
}

export async fn runInterfaces(address: http.Address, restaurant: ServiceRef<RestaurantApi>): Void throws AppError {
  async let terminal = runTerminalInterface(restaurant)
  async let web = runWebInterface(address, restaurant: restaurant)
  defer {
    cancel terminal, reason: .shutdown
    cancel web, reason: .shutdown
  }
  let (_, _) = try await (terminal, web)
}
