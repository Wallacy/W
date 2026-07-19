// W Working Draft — pseudocódigo pedagógico, não executável.
// Uma facade serializa a authority mutável e serve TUI/HTTP pela mesma API.

import { KitchenApi } from restaurant.kitchen
import { MenuError, MenuItem, place as placeMenuItem } from restaurant.menu
import { Receipt } from restaurant.domain

export enum FrontDeskError: Error {
  menu(MenuError)
  start(StartError)
}

export protocol RestaurantApi {
  fn place(item: MenuItem): Receipt async throws FrontDeskError
}

// Storage e implementação permanecem privados ao módulo. Só RestaurantApi e a
// factory abaixo fazem parte da interface exportada.
object FrontDeskState {
  orders: ServiceHost
  kitchen: ServiceRef<KitchenApi>

  mut fn place(item: MenuItem): Receipt async throws FrontDeskError {
    do {
      return try await placeMenuItem(item, orders: inout orders, kitchen: kitchen)
    } catch let error {
      throw .menu(error)
    }
  }
}

export fn startFrontDesk(
  orders: take ServiceHost,
  kitchen: ServiceRef<KitchenApi>,
  on services: inout ServiceHost,
): ServiceRef<RestaurantApi> async throws FrontDeskError {
  do {
    return try await services.startService(
      FrontDeskState(orders: take orders, kitchen: kitchen),
      as: RestaurantApi,
      scope: .process,
      policy: .serial,
      mailbox: .bounded(items: 256, bytes: 1 MiB),
    )
  } catch let error {
    throw .start(error)
  }
}
