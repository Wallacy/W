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
  async fn place(item: MenuItem): Receipt throws FrontDeskError
}

// Storage e implementação permanecem privados ao módulo. Só RestaurantApi e a
// factory abaixo fazem parte da interface exportada.
service FrontDeskState as RestaurantApi {
  orders: ServiceHost
  kitchen: ServiceRef<KitchenApi>

  mut async fn place(item: MenuItem): Receipt throws FrontDeskError {
    do {
      return try await placeMenuItem(item, orders: inout orders, kitchen: kitchen)
    } catch let error {
      throw .menu(error)
    }
  }
}

export async fn startFrontDesk(
  orders: take ServiceHost,
  kitchen: ServiceRef<KitchenApi>,
  on services: inout ServiceHost,
): ServiceRef<RestaurantApi> throws FrontDeskError {
  do {
    return try await services.startService(
      FrontDeskState(orders: take orders, kitchen: kitchen),
      as: RestaurantApi,
      scope: .process,
      policy: .serial,
      mailbox: .bounded(items: 256, bytes: 1MiB),
    )
  } catch let error {
    throw .start(error)
  }
}
