// W Working Draft — pseudocódigo pedagógico, não executável.
// Uma mesa liga vários pedidos sem esconder concorrência ou alias de `inout`.

import { ServiceHost, ServiceRef } from std.service
import {
  BirthdayTableRequest,
  TableReceipt,
} from restaurant.domain
import { KitchenApi } from restaurant.kitchen
import {
  MenuError,
  cancelMenuOrder,
  openMenuOrder,
  prepareMenuCake,
  prepareMenuSalad,
  prepareMenuSoup,
} from restaurant.menu
import { OrderApi } from restaurant.order_service

// O host é usado sequencialmente para criar três instâncias keyed. Depois disso,
// cada child possui seu próprio OrderApi e os pratos podem progredir juntos.
export fn serveBirthdayTable(
  request: BirthdayTableRequest,
  orders: inout ServiceHost,
  kitchen: ServiceRef<KitchenApi>,
): TableReceipt async throws MenuError {
  let cakeOrder = try await openMenuOrder(request.cake.orderId, on: inout orders)
  let soupOrder = try await openMenuOrder(request.soup.orderId, on: inout orders)
  let saladOrder = try await openMenuOrder(request.salad.orderId, on: inout orders)

  async let cake = prepareMenuCake(request.cake, for: cakeOrder, in: kitchen)
  async let soup = prepareMenuSoup(request.soup, for: soupOrder, in: kitchen)
  async let salad = prepareMenuSalad(request.salad, for: saladOrder, in: kitchen)
  let (cake, soup, salad) = try await (cake, soup, salad)

  return TableReceipt(
    tableId: request.tableId,
    cake: cake,
    soup: soup,
    salad: salad,
  )
}

// OrderApi representa estado externo à árvore lexical. Cancelar child tasks não
// desfaz automaticamente esse estado; a compensação permanece explícita.
export fn cancelBirthdayTable(
  cake: ServiceRef<OrderApi>,
  soup: ServiceRef<OrderApi>,
  salad: ServiceRef<OrderApi>,
): Void async throws MenuError {
  async let cakeCancelled = cancelMenuOrder(cake)
  async let soupCancelled = cancelMenuOrder(soup)
  async let saladCancelled = cancelMenuOrder(salad)
  let (_, _, _) = try await (cakeCancelled, soupCancelled, saladCancelled)
}
