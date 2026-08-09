// PYN4 tensor carrier oracle for the Última Luz.
//
// The host machines model DLPack 1.3 lifecycle and device contracts. They do
// not execute W, allocate device memory, call Python, or start a provider.

import tensor from std
import dlpack from std

export type ScientificBatch<samples: usize> =
  Tensor<f32, shape: [samples, 6]>

export struct BlackHoleSensor {
  distance: f64
  watcher: String
}

export enum ScoreError: Error {
  invalidSensor
}

export enum InteropError: Error {
  carrier(dlpack.DLPackError)
  view(dlpack.ViewError<ScoreError>)
}

export struct Scores {
  samples: usize
  anomaly: f32
}

export struct InteropLimits {
  dlpack: dlpack.Limits
  tensor: tensor.Limits
}

export async fn importScientific<samples: usize>(
  take managed: dlpack.ManagedTensor,
  on queue: ref tensor.Queue,
  limits: ref dlpack.Limits,
): dlpack.ImportedTensor<f32, shape: [samples, 6]> throws dlpack.DLPackError {
  return try await dlpack.open(
    managed: take managed,
    on: ref queue,
    limits: ref limits,
  )
}

export fn scoreSamples(
  sensor: ref BlackHoleSensor,
  samples: usize,
): Scores {
  return Scores(samples: samples, anomaly: sensor.distance.toF32())
}

export fn scientificShape(samples: usize): Array<usize> {
  return [samples, 6]
}

export fn sameProviderIdentity(
  leftProvider: String,
  leftId: usize,
  rightProvider: String,
  rightId: usize,
): Bool {
  return leftProvider == rightProvider && leftId == rightId
}

export mut async fn scoreView<samples: usize>(
  view: view Tensor<f32, shape: [samples, 6]>,
  sensor: ref BlackHoleSensor,
): Scores throws ScoreError {
  // The callback returns an owned score. It never returns or stores `view`.
  return scoreSamples(sensor: ref sensor, samples: samples)
}

export mut async fn scoreScientific<samples: usize>(
  imported: inout dlpack.ImportedTensor<f32, shape: [samples, 6]>,
  sensor: ref BlackHoleSensor,
): Scores throws dlpack.ViewError<ScoreError> {
  return try await imported.withView(
    body: scoreView(samples: samples, sensor: ref sensor),
  )
}

export async fn materializeToHost<samples: usize>(
  take managed: dlpack.ManagedTensor,
  target: ref tensor.Device,
  on queue: ref tensor.Queue?,
  limits: ref InteropLimits,
): Tensor<f32, shape: [samples, 6]> throws dlpack.DLPackError {
  return try await dlpack.materialize(
    managed: take managed,
    target: target,
    queue: queue,
    limits: ref limits.dlpack,
  )
}

export async fn exportScores<samples: usize>(
  take scores: Tensor<f32, shape: [samples, 6]>,
  on queue: ref tensor.Queue?,
  limits: ref dlpack.Limits,
): dlpack.ManagedTensor throws dlpack.DLPackError {
  return try await dlpack.export(
    value: take scores,
    queue: queue,
    limits: ref limits,
  )
}

export async fn scientificRoute<samples: usize>(
  take managed: dlpack.ManagedTensor,
  device: ref tensor.Device,
  queue: ref tensor.Queue,
  sensor: ref BlackHoleSensor,
  limits: ref InteropLimits,
): Scores throws InteropError {
  guard queue.device().same(as: ref device) else throw .carrier(.deviceMismatch)

  var imported: dlpack.ImportedTensor<f32, shape: [samples, 6]>
  do {
    imported = try await importScientific(
      samples: samples,
      managed: take managed,
      queue: ref queue,
      limits: ref limits.dlpack,
    )
  } catch error {
    throw .carrier(error)
  }
  defer async {
    do {
      try await imported.close()
    } catch error {
      Trace.current.recordCleanupError(error)
    }
  }
  do {
    return try await scoreScientific(
      samples: samples,
      imported: inout imported,
      sensor: ref sensor,
    )
  } catch error {
    throw .view(error)
  }
}

test "score keeps the sample count and black-hole signal" for scoreSamples {
  let sensor = BlackHoleSensor(distance: 42.5, watcher: "singularity")
  let result = scoreSamples(sensor: ref sensor, samples: 4)
  expect result.samples == 4
  expect result.anomaly == 42.5.toF32()
}

test "shape preserves the six scientific features" for scientificShape {
  expect scientificShape(samples: 4) == [4, 6]
}

test "provider identity is part of device equality" for sameProviderIdentity {
  expect sameProviderIdentity(
    leftProvider: "cuda-a",
    leftId: 0,
    rightProvider: "cuda-b",
    rightId: 0,
  ) == false
  expect sameProviderIdentity(
    leftProvider: "cuda-a",
    leftId: 0,
    rightProvider: "cuda-a",
    rightId: 0,
  ) == true
}
