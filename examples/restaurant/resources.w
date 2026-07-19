// W Working Draft — pseudocódigo pedagógico, não executável.
// Protocols modelam capabilities; implementações e placement físico são do host.

import { CakeFlavor, SaladKind, SoupKind } from restaurant.domain
import { Temperature } from restaurant.units

export enum Ingredient {
  flour
  sugar
  eggs
  butter
  milk
  cocoa
  vegetables
  fruit
  seasoning
}

export struct CakePlan {
  flavor: CakeFlavor
  portions: Int
  temperature: Temperature
  ingredients: List<Ingredient>
}

export struct IngredientBatch {
  items: List<Ingredient>
}

export struct CakeIngredients {
  dry: IngredientBatch
  wet: IngredientBatch
  icing: IngredientBatch
}

export struct SoupIngredients {
  kind: SoupKind
  batch: IngredientBatch
}

export struct SaladIngredients {
  kind: SaladKind
  batch: IngredientBatch
}

export struct Batter {
  dry: IngredientBatch
  wet: IngredientBatch
}

export struct BatterBatch {
  value: Batter
}

export struct Icing {
  batch: IngredientBatch
}

export struct CakeLayer {
  index: Int
}

export struct OvenReady {
  leftKitchen: ServiceRef<OvenLaneApi>
  rightKitchen: ServiceRef<OvenLaneApi>
}

export enum PantryError: Error {
  overloaded
  missing(Ingredient)
  unavailable
}

export enum OvenError: Error {
  overloaded
  unavailable
  temperatureRejected(Temperature)
}

// A lease é um capability owned local. Preaquecer suspende; release é cleanup
// local e idempotente. O formato definitivo dessa API ainda é candidato.
export protocol OvenLease {
  async fn preheat(): OvenReady throws OvenError
  fn release(): Void
}

export protocol OvenLaneApi {
  async fn bake(batch: take BatterBatch): CakeLayer throws OvenError
}

export protocol OvenPoolApi {
  async fn reserve(temperature: Temperature): OvenLease throws OvenError
}

export protocol PantryApi {
  async fn fetchCake(plan: ref CakePlan): CakeIngredients throws PantryError
  async fn fetchSoup(kind: SoupKind, portions: Int): SoupIngredients throws PantryError
  async fn fetchSalad(kind: SaladKind, portions: Int): SaladIngredients throws PantryError
}
