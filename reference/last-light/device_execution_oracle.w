// DEV0 structured accelerator scope for the Last Light observatory.
//
// The host oracle models lifecycle and provider evidence. It does not execute
// W, submit a kernel, allocate device memory, or call a driver.

module device_execution_oracle<
  domains: [
    .accelerated(
      .inference,
      submission: .concurrent,
      maximum: 8,
      fallback: .reject,
    ),
  ],
>

import accelerator from std
import tensor from std
import {
  FeatureBatch,
  WeightMatrix,
  forecastKernel,
  lastLightKernels,
} from ai_harness

export enum DeviceInvocationPhase {
  staged
  submitted
  deviceRunning
  bodySettled
  providerDrained
  cleanup
  outcomeCommitted
  joined
}

export enum ForecastComparison {
  equivalent
  outsideTolerance
}

export fn nextDeviceInvocationPhase(
  phase: DeviceInvocationPhase,
): DeviceInvocationPhase? {
  switch phase {
    case .staged: return .submitted
    case .submitted: return .deviceRunning
    case .deviceRunning: return .bodySettled
    case .bodySettled: return .providerDrained
    case .providerDrained: return .cleanup
    case .cleanup: return .outcomeCommitted
    case .outcomeCommitted: return .joined
    case .joined: return null
  }
}

export async fn forecastOnDevice<
  rows: usize,
  inputs: usize,
  outputs: usize,
>(
  features: ref FeatureBatch<rows: rows, columns: inputs>,
  weights: ref WeightMatrix<inputs: inputs, outputs: outputs>,
): FeatureBatch<rows: rows, columns: outputs> throws accelerator.LaunchError {
  let prediction = spawn<.inference> lastLightKernels.forecast(
    features: ref features,
    weights: ref weights,
  )
  return try await prediction
}

export async fn prepareForecastInputs<
  rows: usize,
  inputs: usize,
  outputs: usize,
>(
  features: take FeatureBatch<rows: rows, columns: inputs>,
  weights: take WeightMatrix<inputs: inputs, outputs: outputs>,
  limits: ref tensor.Limits,
): (
  FeatureBatch<rows: rows, columns: inputs>,
  WeightMatrix<inputs: inputs, outputs: outputs>,
) throws tensor.TensorError {
  let deviceFeatures = try await tensor.transfer<to: .inference>(
    source: take features,
    limits: ref limits,
  )
  let deviceWeights = try await tensor.transfer<to: .inference>(
    source: take weights,
    limits: ref limits,
  )
  return (deviceFeatures, deviceWeights)
}

export async fn forecastOnSelectedDevice<
  rows: usize,
  inputs: usize,
  outputs: usize,
>(
  features: ref FeatureBatch<rows: rows, columns: inputs>,
  weights: ref WeightMatrix<inputs: inputs, outputs: outputs>,
  queue: ref tensor.Queue,
  limits: ref accelerator.Limits,
): FeatureBatch<rows: rows, columns: outputs> throws accelerator.LaunchError {
  var launch = try await accelerator.open(
    module: ref lastLightKernels,
    on: ref queue,
    limits: ref limits,
  )
  defer async {
    do {
      try await (take launch).close()
    } catch error {
      Trace.current.recordCleanupError(error)
    }
  }

  let prediction = async lastLightKernels.forecast.launch(
    using: ref launch,
    features: ref features,
    weights: ref weights,
  )
  return try await prediction
}

export fn forecastOnCpu<
  rows: usize,
  inputs: usize,
  outputs: usize,
>(
  features: ref FeatureBatch<rows: rows, columns: inputs>,
  weights: ref WeightMatrix<inputs: inputs, outputs: outputs>,
): FeatureBatch<rows: rows, columns: outputs> {
  return forecastKernel(features: ref features, weights: ref weights)
}

export async fn copyForecastToHost<
  rows: usize,
  outputs: usize,
>(
  result: take FeatureBatch<rows: rows, columns: outputs>,
  host: ref tensor.Device,
  on queue: ref tensor.Queue?,
  limits: ref tensor.Limits,
): FeatureBatch<rows: rows, columns: outputs> throws tensor.TensorError {
  return try await tensor.transfer(
    source: take result,
    target: ref host,
    queue: queue,
    limits: ref limits,
  )
}

export fn compareStrictForecast(
  cpu hostValue: f32,
  device deviceValue: f32,
): ForecastComparison {
  return if hostValue == deviceValue { .equivalent } else { .outsideTolerance }
}

test "device invocation lifecycle closes in order" for nextDeviceInvocationPhase {
  expect nextDeviceInvocationPhase(phase: .staged) == .submitted
  expect nextDeviceInvocationPhase(phase: .providerDrained) == .cleanup
  expect nextDeviceInvocationPhase(phase: .joined) == null
}

test "strict comparison requires equal values" for compareStrictForecast {
  expect compareStrictForecast(cpu: 42.0, device: 42.0) == .equivalent
  expect compareStrictForecast(cpu: 42.0, device: 42.5) == .outsideTolerance
}
