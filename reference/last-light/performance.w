// Proof-driven performance cases for the Last Light restaurant.

import { Tensor } from std.tensor
import { ReductionMode, Simd, SimdMask } from std.simd
import { ServiceStage } from domain

export type FlavorSignal = Int<(1...128)>
export type FlavorPair = Int<(2...256)>
export type WireName = String<(.bytes.count <= 64)>
export type ScalarName = String<(.scalars.count <= 64)>
export type DisplayName = String<(.graphemes.count <= 64)>
export type RestaurantMenuBytes = Array<u8><(.count in 16...32)>

export struct MenuScanResult {
  fullMatches: UInt
  tailMatches: UInt
  tailLive: SimdMask<16>
}

// SIMD1 design oracle only. It does not claim compiler, runtime or provider.
export fn scanMenuDelimiters(
  menu: ref RestaurantMenuBytes,
  delimiter: u8,
): MenuScanResult {
  let delimiterVector = Simd<u8, lanes: 16>.splat(delimiter)
  let lfVector = Simd<u8, lanes: 16>.splat(10)
  let full = Simd<u8, lanes: 16>.load(from: menu, at: 0)
  let fullMatches = full.equalLanes(delimiterVector) | full.equalLanes(lfVector)
  let (tail, tailLive) = Simd<u8, lanes: 16>.loadPartial(
    from: menu,
    at: 16,
    fill: delimiter,
  )
  let tailMatches = (tail.equalLanes(delimiterVector) | tail.equalLanes(lfVector)) & tailLive
  return MenuScanResult(
    fullMatches: fullMatches.countTrue(),
    tailMatches: tailMatches.countTrue(),
    tailLive: tailLive,
  )
}

// The fill byte equals the delimiter. The inactive mask must prevent a false hit.
export fn wrappingByteVectorOracle(): (Simd<u8, lanes: 4>, SimdMask<4>) {
  let left = Simd<u8, lanes: 4>.fromArray([250, 255, 1, 127])
  let right = Simd<u8, lanes: 4>.fromArray([10, 2, 255, 1])
  return left.overflowingAdd(right)
}

export fn duplicateStaticSwizzle(): Simd<u8, lanes: 3> {
  let source = Simd<u8, lanes: 4>.fromArray([10, 20, 30, 40])
  return source.swizzled<indices: [3, 3, 0]>()
}

// Float reduction modes are nominal and required. This witness is design-only.
export fn floatReductionWitness(): (f64, f64) {
  let values = Simd<f64, lanes: 4>.fromArray([0.1, 0.2, 0.3, 0.4])
  let strictMode: ReductionMode = .strict
  return (
    values.reduceAdd(mode: strictMode),
    values.reduceMultiply(mode: .reproducible),
  )
}

export struct BrigadeCount {
  completed: u64
  failed: u64
}

// The compiler may change this private physical layout. Cache placement is not
// part of Atomic<T> or of the logical fields.
object InterferenceCounters {
  var atomic completed: u64 = 0
  var atomic horizonSamples: u64 = 0
}

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

export fn countCompleted(orders: Array<Bool>): BrigadeCount {
  var completed: u64 = 0
  var failed: u64 = 0

  for succeeded in orders {
    if succeeded { completed += 1 }
    else { failed += 1 }
  }

  return BrigadeCount(completed: completed, failed: failed)
}

export fn combineBrigadeCounts(
  left: BrigadeCount,
  right: BrigadeCount,
): BrigadeCount {
  return BrigadeCount(
    completed: left.completed + right.completed,
    failed: left.failed + right.failed,
  )
}

export fn wireName(value: ref String): WireName throws RefinementError {
  return try WireName(value)
}

test "range facts prove the result refinement" for combineFlavor {
  let left = try FlavorSignal(128)
  let right = try FlavorSignal(128)
  let result: FlavorPair = combineFlavor(left: left, right: right)

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

test "partitioned counts combine only at the join" for combineBrigadeCounts {
  let port = countCompleted(orders: [true, true, false])
  let starboard = countCompleted(orders: [false, true])
  let total = combineBrigadeCounts(left: port, right: starboard)

  expect total.completed == 3
  expect total.failed == 2
}

test "SIMD delimiter scan masks a delimiter fill in the tail" for scanMenuDelimiters {
  let menu: RestaurantMenuBytes = [
    82, 101, 115, 116, 97, 117, 114, 97, 110, 116, 124, 10, 101, 110, 117, 124,
    83, 101, 10, 124,
  ]
  let result = scanMenuDelimiters(menu: ref menu, delimiter: 124)

  expect result.fullMatches == 3
  expect result.tailMatches == 2
  expect result.tailLive.countTrue() == 4
}

test "SIMD overflowing integer and duplicate swizzle are explicit" {
  let (wrapped, overflowed) = wrappingByteVectorOracle()
  expect wrapped.toArray() == [4, 1, 0, 128]
  expect overflowed.toArray() == [true, true, true, false]

  let duplicate = duplicateStaticSwizzle()
  expect duplicate.toArray() == [40, 40, 10]
}

test "SIMD float reductions name their mode" for floatReductionWitness {
  let (sum, product) = floatReductionWitness()
  expect sum > 0.9_f64
  expect product > 0.0_f64
}
