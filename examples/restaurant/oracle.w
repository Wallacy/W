// Shape-aware planning for the Oráculo de Mesas.

import std.tensor
import { Course, Probability } from restaurant.domain

export enum OracleError: Error {
  invalidShape(ShapeError)
  nonFiniteScore
  emptyBatch
}

export struct Forecast<const tables: usize, const courses: usize> {
  demand: Tensor<f32, shape: [tables, courses]>
  confidence: Tensor<f32, shape: [tables, courses]>
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
