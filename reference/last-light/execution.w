// Bounded parallel work for the Observatory of the Patient Comet.

module execution<
  domains: [
    .concurrent(.thermal, maximum: 4, capabilities: [.parallel]),
  ],
>

import { OrderId } from domain
import { Ingredient, KitchenError, Mixture, Recipe } from kitchen

export type BatchIndex = usize
export alias ClosingTimeout = TaskTimeout

export protocol CompletionMetric {
  completionCount: u64 { get }
}

export object BrigadeMetrics: CompletionMetric {
  var atomic completed: u64 = 0

  export init() {}

  fn recordCompletion() {
    completed.saturatingAdd<.relaxed>(1)
  }

  completionCount: u64 {
    get => completed.load<.relaxed>()
  }
}

export struct MixingJob {
  index: BatchIndex
  orderId: OrderId
  ingredients: Array<Ingredient>
  recipe: Recipe
  metrics: shared BrigadeMetrics
}

export struct MixingResult {
  index: BatchIndex
  orderId: OrderId
  mixture: Mixture
}

export enum BrigadeError: Error {
  kitchen(KitchenError)
  invalidParallelism(found: usize, maximum: usize)
}

export enum LastBellResult {
  mixed(MixingResult)
  failed(BrigadeError)
  timedOut
  runtimePressure
  canceled
}

const maximumParallelCooks: usize = 256

fn mixJob(job: take MixingJob): MixingResult throws BrigadeError {
  guard !job.ingredients.isEmpty else throw .kitchen(.emptyStock)

  let mixture = Mixture(
    course: job.recipe.course,
    mass: job.ingredients.totalMass(),
    homogeneity: job.ingredients.homogeneity(),
  )

  job.metrics.recordCompletion()
  return MixingResult(index: job.index, orderId: job.orderId, mixture: mixture)
}

async fn mixCooperatively(job: take MixingJob): MixingResult throws BrigadeError {
  Task.checkCancellation()
  await Task.yield()
  Task.checkCancellation()
  return mixJob(take job)
}

export async fn mixPair(
  left: take MixingJob,
  right: take MixingJob,
): (MixingResult, MixingResult) throws BrigadeError {
  spawn<.compute> let leftResult = mixJob(take left)
  spawn<.compute> let rightResult = mixJob(take right)
  return try await (leftResult, rightResult)
}

export async fn mixOnThermalLane(job: take MixingJob): MixingResult throws BrigadeError {
  spawn<.thermal> let result = mixJob(take job)
  return try await result
}

export async fn mixBatch(
  jobs: take Array<MixingJob>,
  parallelism: usize,
): Array<MixingResult> throws BrigadeError {
  guard parallelism > 0 && parallelism <= maximumParallelCooks else {
    throw .invalidParallelism(found: parallelism, maximum: maximumParallelCooks)
  }

  let worker: fn(take MixingJob): MixingResult throws BrigadeError = mixJob

  return try await TaskGroup.parallelMap<.compute>(
    take jobs,
    limit: parallelism,
    ordering: .input,
    using: worker,
  )
}

// Both batches create nested compute groups. They share one domain budget, so
// their limits bound task resources without multiplying the worker count.
export async fn mixAcrossTwoKitchens(
  portJobs: take Array<MixingJob>,
  starboardJobs: take Array<MixingJob>,
  parallelismPerKitchen: usize,
): (Array<MixingResult>, Array<MixingResult>) throws BrigadeError {
  spawn<.compute> let port = mixBatch(take portJobs, parallelism: parallelismPerKitchen)
  spawn<.compute> let starboard = mixBatch(take starboardJobs, parallelism: parallelismPerKitchen)
  return try await (port, starboard)
}

export async fn inspectEveryFailure(
  jobs: take Array<MixingJob>,
  parallelism: usize,
): Array<TaskOutcome<MixingResult, BrigadeError>> throws BrigadeError {
  guard parallelism > 0 && parallelism <= maximumParallelCooks else {
    throw .invalidParallelism(found: parallelism, maximum: maximumParallelCooks)
  }

  return await TaskGroup.parallelCollect<.compute>(
    take jobs,
    limit: parallelism,
    ordering: .input,
    using: mixJob,
  )
}

export async fn closeBeforeTheLastCourse(
  jobs: take Array<MixingJob>,
  parallelism: usize,
): TaskOutcome<Array<MixingResult>, BrigadeError> {
  async let batch = mixBatch(take jobs, parallelism: parallelism)
  batch.cancel(reason: .shutdown)
  return await batch.outcome()
}

export async fn mixBeforeTheLastBell(
  job: take MixingJob,
  timeout: ClosingTimeout,
): TaskOutcome<MixingResult, BrigadeError> {
  return await Task.withTimeout(
    for: timeout,
    input: take job,
    using: mixCooperatively,
  )
}

fn cancellationResult(cancellation: ref Cancellation): LastBellResult {
  if cancellation.deadlineExceeded {
    return .timedOut
  }

  if cancellation.exceeded(.liveTasks)
    || cancellation.exceeded(.taskFrameBytes)
    || cancellation.exceeded(.timers)
    || cancellation.exceeded(.readyJobs) {
    return .runtimePressure
  }

  return .canceled
}

export fn explainLastBell(
  outcome: take TaskOutcome<MixingResult, BrigadeError>,
): LastBellResult {
  return switch take outcome {
    case .success(let result): .mixed(take result)
    case .error(let error): .failed(error)
    case .canceled(let cancellation): cancellationResult(cancellation)
  }
}
