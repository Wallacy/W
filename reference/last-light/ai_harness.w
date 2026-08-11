// Host/device tensor contracts for forecasting and anomaly detection.

import accelerator from std
import { Tensor } from std.tensor

export type FeatureBatch<rows: usize, columns: usize> =
  Tensor<f32, shape: [rows, columns]>

export type WeightMatrix<inputs: usize, outputs: usize> =
  Tensor<f32, shape: [inputs, outputs]>

export struct TrainingBatch<
  rows: usize<(1...)>,
  inputs: usize,
  outputs: usize,
> {
  features: FeatureBatch<rows: rows, columns: inputs>
  labels: FeatureBatch<rows: rows, columns: outputs>
}

export struct TrainingMetrics {
  meanSquaredError: f32
}

export fn forecastKernel<
  rows: usize,
  inputs: usize,
  outputs: usize,
>(
  features: ref FeatureBatch<rows: rows, columns: inputs>,
  weights: ref WeightMatrix<inputs: inputs, outputs: outputs>,
): FeatureBatch<rows: rows, columns: outputs> {
  return features @ weights
}

export fn normalizeKernel<rows: usize, columns: usize>(
  values: inout FeatureBatch<rows: rows, columns: columns>,
) {
  let totals = values.sum(axis: 1, mode: .reproducible)
  values /= totals.broadcast(to: [rows, columns])
}

export fn trainLinearKernel<
  rows: usize<(1...)>,
  inputs: usize,
  outputs: usize,
>(
  weights: inout WeightMatrix<inputs: inputs, outputs: outputs>,
  batch: ref TrainingBatch<rows: rows, inputs: inputs, outputs: outputs>,
  learningRate: f32<(0.0>..<1.0)>,
): TrainingMetrics {
  let prediction = batch.features @ weights
  let error = prediction - batch.labels
  let gradient = batch.features.transposed() @ error / rows.toF32()
  weights -= gradient * learningRate

  return TrainingMetrics(
    meanSquaredError: (error * error).mean(mode: .reproducible),
  )
}

export const lastLightKernels = accelerator.module<{
  forecast: forecastKernel,
  normalize: normalizeKernel,
  trainLinear: trainLinearKernel,
}>()

test "matrix contraction fixes the output shape" for forecastKernel {
  let features: FeatureBatch<rows: 2, columns: 3> = [
    [1.0, 2.0, 3.0],
    [4.0, 5.0, 6.0],
  ]
  let weights: WeightMatrix<inputs: 3, outputs: 1> = [
    [1.0],
    [0.5],
    [0.25],
  ]
  let result = forecastKernel(features, weights: weights)
  expect result.shape == [2, 1]
}
