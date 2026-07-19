// W Working Draft — pseudocódigo pedagógico, não executável.
// Cada try await entre ServiceRefs mantém suspensão/falha/custo observáveis.

import {
  Cake,
  CakeFlavor,
  CakeRequest,
  DishSummary,
  Salad,
  SaladKind,
  SaladRequest,
  Soup,
  SoupKind,
  SoupRequest,
} from restaurant.domain
import {
  Batter,
  BatterBatch,
  CakeIngredients,
  CakeLayer,
  CakePlan,
  Icing,
  IngredientBatch,
  OvenError,
  OvenLease,
  OvenLaneApi,
  OvenPoolApi,
  OvenReady,
  PantryApi,
  PantryError,
  SaladIngredients,
  SoupIngredients,
} from restaurant.resources
import { cakeSetpoint } from restaurant.oven
import { Temperature } from restaurant.units
import { OrderApi, OrderError, OrderStage } from restaurant.order_service

export protocol KitchenApi {
  fn makeCake(request: CakeRequest, for order: ServiceRef<OrderApi>): Cake async throws KitchenError
  fn makeSoup(request: SoupRequest, for order: ServiceRef<OrderApi>): Soup async throws KitchenError
  fn makeSalad(request: SaladRequest, for order: ServiceRef<OrderApi>): Salad async throws KitchenError
}

export enum KitchenError: Error {
  overloaded
  invalidRecipe(String)
  cancelled
  pantry(PantryError)
  oven(OvenError)
  order(OrderError)
}

fn moveOrder(order: ServiceRef<OrderApi>, to stage: OrderStage): Void async throws KitchenError {
  do {
    return try await order.move(to: stage)
  } catch let error {
    throw .order(error)
  }
}

fn validateCake(request: CakeRequest): CakePlan throws KitchenError {
  guard request.portions > 0 && request.portions <= 24 else {
    throw .invalidRecipe("Cake portions must be between 1 and 24")
  }

  return CakePlan(
    flavor: request.flavor,
    portions: request.portions,
    temperature: cakeSetpoint(request.flavor),
    ingredients: [.flour, .sugar, .eggs, .butter, .milk],
  )
}

fn reserveOven(temperature: Temperature, in ovens: ServiceRef<OvenPoolApi>): OvenLease async throws KitchenError {
  do {
    return try await ovens.reserve(temperature)
  } catch let error {
    throw .oven(error)
  }
}

fn preheatOven(lease: ref OvenLease): OvenReady async throws KitchenError {
  do {
    return try await lease.preheat()
  } catch let error {
    throw .oven(error)
  }
}

fn fetchCakeIngredients(
  plan: ref CakePlan,
  from pantry: ServiceRef<PantryApi>,
): CakeIngredients async throws KitchenError {
  do {
    return try await pantry.fetchCake(plan)
  } catch let error {
    throw .pantry(error)
  }
}

fn fetchSoupIngredients(
  request: SoupRequest,
  from pantry: ServiceRef<PantryApi>,
): SoupIngredients async throws KitchenError {
  do {
    return try await pantry.fetchSoup(request.kind, portions: request.portions)
  } catch let error {
    throw .pantry(error)
  }
}

fn fetchSaladIngredients(
  request: SaladRequest,
  from pantry: ServiceRef<PantryApi>,
): SaladIngredients async throws KitchenError {
  do {
    return try await pantry.fetchSalad(request.kind, portions: request.portions)
  } catch let error {
    throw .pantry(error)
  }
}

fn mix(dry: take IngredientBatch, with wet: take IngredientBatch): Batter {
  return Batter(dry: take dry, wet: take wet)
}

fn prepareIcing(batch: take IngredientBatch): Icing {
  return Icing(batch: take batch)
}

fn splitBatter(batter: take Batter): (BatterBatch, BatterBatch) {
  return (BatterBatch(value: copy batter), BatterBatch(value: take batter))
}

fn bakeLayer(batch: take BatterBatch, in lane: ServiceRef<OvenLaneApi>): CakeLayer async throws KitchenError {
  do {
    return try await lane.bake(take batch)
  } catch let error {
    throw .oven(error)
  }
}

fn cakeLabel(flavor: CakeFlavor): String {
  switch flavor {
    case .chocolate:
      return "chocolate cake"
    case .vanilla:
      return "vanilla cake"
    case .carrot:
      return "carrot cake"
  }
}

fn soupLabel(kind: SoupKind): String {
  switch kind {
    case .tomato:
      return "tomato soup"
    case .onion:
      return "onion soup"
    case .pumpkin:
      return "pumpkin soup"
  }
}

fn saladLabel(kind: SaladKind): String {
  switch kind {
    case .garden:
      return "garden salad"
    case .caesar:
      return "caesar salad"
    case .fruit:
      return "fruit salad"
  }
}

fn decorate(left: take CakeLayer, right: take CakeLayer, with icing: take Icing, using plan: CakePlan): Cake {
  return Cake(summary: DishSummary(kind: .cake, portions: plan.portions, label: cakeLabel(plan.flavor)))
}

fn package(cake: take Cake): Cake {
  return cake
}

fn cookSoup(ingredients: take SoupIngredients, portions: Int): Soup {
  return Soup(summary: DishSummary(kind: .soup, portions: portions, label: soupLabel(ingredients.kind)))
}

fn assembleSalad(ingredients: take SaladIngredients, portions: Int): Salad {
  return Salad(summary: DishSummary(kind: .salad, portions: portions, label: saladLabel(ingredients.kind)))
}

object KitchenState {
  pantry: ServiceRef<PantryApi>
  ovens: ServiceRef<OvenPoolApi>

  mut fn makeSoup(request: SoupRequest, for order: ServiceRef<OrderApi>): Soup async throws KitchenError {
    try await moveOrder(order, to: .preparing)
    let ingredients = try await fetchSoupIngredients(request, from: pantry)
    Task.checkCancellation()
    let soup = cookSoup(take ingredients, portions: request.portions)
    try await moveOrder(order, to: .finishing)
    return soup
  }

  mut fn makeSalad(request: SaladRequest, for order: ServiceRef<OrderApi>): Salad async throws KitchenError {
    try await moveOrder(order, to: .preparing)
    let ingredients = try await fetchSaladIngredients(request, from: pantry)
    Task.checkCancellation()
    let salad = assembleSalad(take ingredients, portions: request.portions)
    try await moveOrder(order, to: .finishing)
    return salad
  }

  mut fn makeCake(request: CakeRequest, for order: ServiceRef<OrderApi>): Cake async throws KitchenError {
    // Sequencial: validar antes de consumir capacidade limitada.
    let plan = try validateCake(request)
    // reserve suspende de modo cancelável ou retorna overload; nunca drop.
    let slot = try await reserveOven(plan.temperature, in: ovens)
    defer { slot.release() } // sucesso, error e cancelamento limpam o recurso.

    try await moveOrder(order, to: .preparing)

    // Concorrência durante espera; ambos continuam filhos deste handler.
    async let ovenTask = preheatOven(slot)
    async let ingredientsTask = fetchCakeIngredients(plan, from: pantry)
    let (oven, ingredients) = try await (ovenTask, ingredientsTask)

    // Assistants são child tasks estruturadas, não trabalho destacado. Mover
    // fields distintos em paralelo é um caso deliberado para W-O002.
    async let batterTask = mix(take ingredients.dry, with: ingredients.wet)
    async let icingTask = prepareIcing(take ingredients.icing)
    let (batter, icing) = await (batterTask, icingTask)

    try await moveOrder(order, to: .baking)

    // Paralelismo explícito: lotes independentes, dados transferidos.
    let (left, right) = splitBatter(take batter)
    spawn let cookA = bakeLayer(take left, in: oven.leftKitchen)
    spawn let cookB = bakeLayer(take right, in: oven.rightKitchen)
    let (leftLayer, rightLayer) = try await (cookA, cookB)

    Task.checkCancellation()
    try await moveOrder(order, to: .finishing)

    // Sequencial novamente: decorar depende das duas camadas assadas.
    let cake = decorate(take leftLayer, take rightLayer, with: take icing, using: plan)
    return package(take cake)
  }
}
