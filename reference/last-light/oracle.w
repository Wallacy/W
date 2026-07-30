// Shape-aware planning for the Oráculo de Mesas.

import std.tensor
import { Course, Order, Probability } from restaurant.domain
import { Ingredient, KitchenPlan, Recipe } from restaurant.kitchen
import { serviceTemperature } from restaurant.units

export enum OracleError: Error {
  invalidShape(ShapeError)
  nonFiniteScore
  emptyBatch
  missingRecipe(Course)
}

export struct Forecast<const tables: usize, const courses: usize> {
  demand: Tensor<f32, shape: [tables, courses]>
  confidence: Tensor<Probability, shape: [tables, courses]>
}

export struct PlanningRequest {
  features: Tensor<f32, shape: [1, 8]>
}

export protocol OracleApi {
  async fn plan(request: take PlanningRequest): KitchenPlan throws OracleError
}

export fn planningRequest(order: ref Order): PlanningRequest {
  return PlanningRequest(
    features: [[
      guests.toF32(),
      course.ordinal.toF32(),
      notes?.scalars.count.toF32() ?? 0.0,
      guest.name.scalars.count.toF32(),
      1.0,
      0.0,
      0.0,
      1.0,
    ]],
  )
}

fn normalized<const rows: usize, const columns: usize>(
  values: ref Tensor<f32, shape: [rows, columns]>,
): Tensor<f32, shape: [rows, columns]> throws OracleError {
  let totals = values.sum(axis: 1, mode: .reproducible)
  guard totals.all((value) => value.isFinite && value > 0.0) else {
    throw .nonFiniteScore
  }

  return values / totals.broadcast(to: [rows, columns])
}

export fn forecast<const tables: usize, const features: usize, const courses: usize>(
  observations: ref Tensor<f32, shape: [tables, features]>,
  weights: ref Tensor<f32, shape: [features, courses]>,
): Forecast<tables: tables, courses: courses> throws OracleError {
  guard tables > 0 else throw .emptyBatch

  let logits = observations @ weights
  let demand = try normalized(logits.softmax(axis: 1))
  let confidence = try demand.map((value) => try Probability(value))

  return Forecast(demand: demand, confidence: confidence)
}

fn defaultWeights(): Tensor<f32, shape: [8, 4]> {
  return [
    [0.8, 0.2, 0.1, 0.4],
    [0.1, 0.9, 0.3, 0.2],
    [0.3, 0.2, 0.8, 0.4],
    [0.4, 0.3, 0.2, 0.9],
    [0.2, 0.3, 0.4, 0.5],
    [0.5, 0.4, 0.3, 0.2],
    [0.6, 0.2, 0.5, 0.3],
    [0.3, 0.5, 0.2, 0.6],
  ]
}

fn defaultRecipes(): Map<Course, Recipe> {
  return [
    .nebulaBroth: Recipe(
      course: .nebulaBroth,
      ingredients: [.quietWater, .ionizedSugar],
      target: serviceTemperature,
      duration: 12<si.s>,
      energyBudget: 360_000<si.J>,
    ),
    .photonSouffle: Recipe(
      course: .photonSouffle,
      ingredients: [.cometFlour, .vacuumButter, .ionizedSugar],
      target: serviceTemperature,
      duration: 18<si.s>,
      energyBudget: 540_000<si.J>,
    ),
    .quietSalad: Recipe(
      course: .quietSalad,
      ingredients: [.horizonFruit, .quietWater],
      target: serviceTemperature,
      duration: 4<si.s>,
      energyBudget: 120_000<si.J>,
    ),
    .horizonCake: Recipe(
      course: .horizonCake,
      ingredients: [
        .cometFlour,
        .vacuumButter,
        .ionizedSugar,
        .horizonFruit,
      ],
      target: serviceTemperature,
      duration: 42<si.s>,
      energyBudget: 1_260_000<si.J>,
    ),
  ]
}

export service TableOracle as OracleApi {
  weights: Tensor<f32, shape: [8, 4]> = defaultWeights()
  recipes: Map<Course, Recipe> = defaultRecipes()

  async fn plan(request: take PlanningRequest): KitchenPlan throws OracleError {
    let prediction = try forecast(request.features, weights: weights)
    let courseIndex = prediction.demand[0].argmax(mode: .reproducible)
    let course = Course.fromOrdinal(courseIndex)
    guard let ref recipe = recipes[course] else throw .missingRecipe(course)

    return KitchenPlan(
      recipe: copy recipe,
      minimumAroma: prediction.confidence[0, courseIndex],
      duration: recipe.duration,
      energyBudget: recipe.energyBudget,
    )
  }
}

test "matrix contraction preserves the declared shape" for forecast {
  let observations: Tensor<f32, shape: [2, 3]> = [
    [1.0, 0.0, 0.5],
    [0.2, 0.8, 0.0],
  ]

  let weights: Tensor<f32, shape: [3, 4]> = [
    [0.9, 0.1, 0.2, 0.4],
    [0.1, 0.8, 0.3, 0.2],
    [0.4, 0.2, 0.7, 0.1],
  ]

  let result = try forecast(observations, weights: weights)
  expect result.demand.shape == [2, 4]
}
