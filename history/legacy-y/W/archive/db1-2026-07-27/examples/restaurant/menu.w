// W Working Draft — pseudocódigo pedagógico, não executável.
// Formas de service/runtime abaixo são candidatas, não sintaxe normativa.

import {
  Cake,
  CakeRequest,
  DishSummary,
  OrderId,
  Receipt,
  Salad,
  SaladRequest,
  Soup,
  SoupRequest,
} from restaurant.domain
import { OrderApi, OrderError, openOrder } from restaurant.order_service
import { KitchenApi, KitchenError } from restaurant.kitchen

export enum MenuItem {
  cake(CakeRequest)
  soup(SoupRequest)
  salad(SaladRequest)
}

export enum MenuError: Error {
  soldOut(MenuItem)
  order(OrderError)
  kitchen(KitchenError)
}

// Enquanto W-O033 estiver aberta, cada boundary converte seu error set de modo
// explícito. `try` sozinho não injeta OrderError/KitchenError em MenuError.
export async fn openMenuOrder(id: OrderId, on host: inout ServiceHost): ServiceRef<OrderApi> throws MenuError {
  do {
    return try await openOrder(id, on: inout host)
  } catch let error {
    throw .order(error)
  }
}

async fn makeMenuCake(
  request: CakeRequest,
  for order: ServiceRef<OrderApi>,
  in kitchen: ServiceRef<KitchenApi>,
): Cake throws MenuError {
  do {
    return try await kitchen.makeCake(request, for: order)
  } catch let error {
    throw .kitchen(error)
  }
}

async fn makeMenuSoup(
  request: SoupRequest,
  for order: ServiceRef<OrderApi>,
  in kitchen: ServiceRef<KitchenApi>,
): Soup throws MenuError {
  do {
    return try await kitchen.makeSoup(request, for: order)
  } catch let error {
    throw .kitchen(error)
  }
}

async fn makeMenuSalad(
  request: SaladRequest,
  for order: ServiceRef<OrderApi>,
  in kitchen: ServiceRef<KitchenApi>,
): Salad throws MenuError {
  do {
    return try await kitchen.makeSalad(request, for: order)
  } catch let error {
    throw .kitchen(error)
  }
}

async fn finishMenuOrder(order: ServiceRef<OrderApi>, with summary: DishSummary): Receipt throws MenuError {
  do {
    return try await order.complete(with: summary)
  } catch let error {
    throw .order(error)
  }
}

export async fn cancelMenuOrder(order: ServiceRef<OrderApi>): Void throws MenuError {
  do {
    return try await order.requestCancellation()
  } catch let error {
    throw .order(error)
  }
}

export async fn prepareMenuCake(
  request: CakeRequest,
  for order: ServiceRef<OrderApi>,
  in kitchen: ServiceRef<KitchenApi>,
): Receipt throws MenuError {
  let cake = try await makeMenuCake(request, for: order, in: kitchen)
  return try await finishMenuOrder(order, with: cake.summary)
}

export async fn prepareMenuSoup(
  request: SoupRequest,
  for order: ServiceRef<OrderApi>,
  in kitchen: ServiceRef<KitchenApi>,
): Receipt throws MenuError {
  let soup = try await makeMenuSoup(request, for: order, in: kitchen)
  return try await finishMenuOrder(order, with: soup.summary)
}

export async fn prepareMenuSalad(
  request: SaladRequest,
  for order: ServiceRef<OrderApi>,
  in kitchen: ServiceRef<KitchenApi>,
): Receipt throws MenuError {
  let salad = try await makeMenuSalad(request, for: order, in: kitchen)
  return try await finishMenuOrder(order, with: salad.summary)
}

// O switch fechado faz cada item escolher um fluxo visível.
export async fn place(
  item: MenuItem,
  orders: inout ServiceHost,
  kitchen: ServiceRef<KitchenApi>, // handle conceitual candidato
): Receipt throws MenuError {
  switch item {
    case .cake(let request):
      let order = try await openMenuOrder(request.orderId, on: inout orders)
      return try await prepareMenuCake(request, for: order, in: kitchen)

    case .soup(let request):
      let order = try await openMenuOrder(request.orderId, on: inout orders)
      return try await prepareMenuSoup(request, for: order, in: kitchen)

    case .salad(let request):
      let order = try await openMenuOrder(request.orderId, on: inout orders)
      return try await prepareMenuSalad(request, for: order, in: kitchen)
  }
}
