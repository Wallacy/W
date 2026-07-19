/// Fixture lexical: não é um programa completo nem define a gramática W.
import { io, Url } from std.net
import kitchen as restaurantKitchen from restaurant.kitchen

@repr(c)
struct Recipe<T> {
  name: String
  style: CakeStyle
  portions: u16
  temperature: f64
  ingredients: T
}

enum KitchenError: Error {
  unavailable
  invalidRecipe(String)
}

enum CakeStyle {
  plain
  layered
}

const maxCooks = 1_024
let endpoint = Url("https://restaurant.example/${maxCooks}")
let rawPath = r"C:\kitchen\${literal}"
let note = """
  Cake service: candidate source.
  """

fn inspect(recipe: ref Recipe<String>): Bool {
  return recipe.portions > 0 && recipe.name != ""
}

fn makeCake(recipe: take Recipe<String>, oven: inout Oven): Cake async throws KitchenError {
  guard inspect(recipe) else {
    throw .invalidRecipe("A recipe needs portions")
  }

  async let preheated = oven.preheat(to: recipe.temperature)
  async let icing = prepareIcing(for: recipe.name)
  let (readyOven, readyIcing) = try await (preheated, icing)

  switch recipe.style {
    case .layered:
      spawn let left = bake(take copy recipe, in: readyOven.left)
      spawn let right = bake(take recipe, in: readyOven.right)
      let layers = try await (left, right)
      return try await assemble(layers, with: readyIcing)
    case .plain:
      return try await bake(take recipe, in: readyOven)
  }
}

// 0xff_u8, 0b1010, 3.141_592_f64, .none, true, false, value?.name ?? "Cake"
