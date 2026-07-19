// W Working Draft — pseudocódigo pedagógico, não executável.
// Namespace `http`/`json` é std implícita; import não concede socket ou rede.

import { Receipt } from restaurant.domain
import { FrontDeskError, RestaurantApi } from restaurant.front_desk
import { MenuItem } from restaurant.menu

export enum WebError: Error {
  decode(json.DecodeError)
  frontDesk(FrontDeskError)
  server(http.ServerError)
}

fn decodeOrder(request: ref http.Request): MenuItem throws WebError {
  do {
    return try json.decode(request.body, as: MenuItem)
  } catch let error {
    throw .decode(error)
  }
}

fn submitOrder(item: MenuItem, to restaurant: ServiceRef<RestaurantApi>): Receipt async throws WebError {
  do {
    return try await restaurant.place(item)
  } catch let error {
    throw .frontDesk(error)
  }
}

export fn handleRequest(
  request: http.Request,
  with restaurant: ServiceRef<RestaurantApi>,
): http.Response async throws WebError {
  if request.method == .get && request.path == "/health" {
    return http.Response.text("ok")
  }

  if request.method == .post && request.path == "/orders" {
    let item = try decodeOrder(request)
    let receipt = try await submitOrder(item, to: restaurant)
    return http.Response.json(receipt, status: .created)
  }

  return http.Response.text("not found", status: .notFound)
}

export fn serveWeb(address: http.Address, restaurant: ServiceRef<RestaurantApi>): Void async throws WebError {
  do {
    return try await http.serve(address, handler: (request) => handleRequest(request, with: restaurant))
  } catch let error {
    throw .server(error)
  }
}
