// Shape-aware planning for the Oráculo de Mesas.

import std.tensor
import { Course, Order, Probability } from restaurant.domain
import { KitchenPlan, Recipe } from restaurant.kitchen

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

export protocol OracleApi {
  async fn plan(order: ref Order): KitchenPlan throws OracleError
}

extension Order {
  fn oracleFeatures(): Tensor<f32, shape: [1, 8]> {
    return [[
      guests.toF32(),
      course.ordinal.toF32(),
      notes?.scalars.count.toF32() ?? 0.0,
      guest.name.scalars.count.toF32(),
      1.0,
      0.0,
      0.0,
      1.0,
    ]]
  }
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
  let confidence = demand.map((value) => try Probability(value))

  return Forecast(demand: demand, confidence: confidence)
}

export service TableOracle as OracleApi {
  weights: Tensor<f32, shape: [8, 4]>
  recipes: Map<Course, Recipe>

  async fn plan(order: ref Order): KitchenPlan throws OracleError {
    let prediction = try forecast(order.oracleFeatures(), weights: weights)
    let courseIndex = prediction.demand[0].argmax(mode: .reproducible)
    let course = Course.fromOrdinal(courseIndex)
    guard let recipe = recipes[course] else throw .missingRecipe(course)

    return KitchenPlan(
      recipe: recipe,
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
