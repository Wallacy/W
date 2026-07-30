// Native training harness that shares kernels with accelerator products.

import {
  TrainingBatch,
  WeightMatrix,
  trainLinearKernel,
} from restaurant.ai_harness

fn runAiLab(args: ProcessArguments, ctx: ProcessContext): ExitCode {
  var weights: WeightMatrix<inputs: 2, outputs: 1> = [
    [0.25],
    [0.75],
  ]
  let batch = TrainingBatch<rows: 2, inputs: 2, outputs: 1>(
    features: [
      [1.0, 0.0],
      [0.0, 1.0],
    ],
    labels: [
      [0.0],
      [1.0],
    ],
  )
  let metrics = trainLinearKernel(inout weights, batch: batch, learningRate: 0.05)

  print("Final-horizon training MSE: ${metrics.meanSquaredError}.")
  return .success
}

entry LastLightAiLab(runAiLab)
