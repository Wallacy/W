// Proof-driven performance cases for the Last Light restaurant.

import { Tensor } from std.tensor
import { ServiceStage } from domain

export type FlavorSignal = Int<(1...128)>
export type FlavorPair = Int<(2...256)>
export type WireName = String<(.bytes.count <= 64)>
export type ScalarName = String<(.scalars.count <= 64)>
export type DisplayName = String<(.graphemes.count <= 64)>

export alias ActiveStage =
  ServiceStage<[.reserving, .preparing, .serving]>

export fn combineFlavor(
  left: FlavorSignal,
  right: FlavorSignal,
): FlavorPair {
  return left + right
}

export fn flavorScore<tables: usize>(
  samples: ref Tensor<FlavorSignal, shape: [tables, 64]>,
  weights: ref Tensor<FlavorSignal, shape: [64, 8]>,
): Tensor<Int, shape: [tables, 8]> {
  return samples @ weights
}

export fn activeStageCode(stage: ActiveStage): u8 {
  return switch stage {
    case .reserving: 1
    case .preparing: 2
    case .serving: 3
  }
}

export fn scaleForecast(
  values: inout Tensor<f32, shape: [128]>,
  factor: f32,
): () {
  values *= factor
}

export fn wireName(value: ref String): WireName throws RefinementError {
  return try WireName(value)
}

test "range facts prove the result refinement" for combineFlavor {
  let left = try FlavorSignal(128)
  let right = try FlavorSignal(128)
  let result: FlavorPair = combineFlavor(left, right: right)

  expect result == 256
}

test "an enum subset removes impossible branches" for activeStageCode {
  expect activeStageCode(.reserving) == 1
  expect activeStageCode(.preparing) == 2
  expect activeStageCode(.serving) == 3
}

test "text refinements expose different capacity facts" {
  let wire = try WireName("Violet Horizon")
  let scalar = try ScalarName("Violet Horizon")
  let display = try DisplayName("Violet Horizon")

  expect wire.bytes.count <= 64
  expect scalar.scalars.count <= 64
  expect display.graphemes.count <= 64
}
