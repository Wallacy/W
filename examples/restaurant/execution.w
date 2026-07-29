// Bounded parallel work for the Observatory of the Patient Comet.

import { OrderId } from restaurant.domain
import { Ingredient, KitchenError, Mixture, Recipe } from restaurant.kitchen

export type BatchIndex = usize

export protocol CompletionMetric {
  completionCount: u64 { get }
}

export object BrigadeMetrics: CompletionMetric {
  var atomic completed: u64 = 0

  export init() {}

  fn recordCompletion() {
    completed += 1
  }

  completionCount: u64 {
    get => completed
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

export async fn mixPair(
  left: take MixingJob,
  right: take MixingJob,
): (MixingResult, MixingResult) throws BrigadeError {
  spawn<.compute> let leftResult = mixJob(take left)
  spawn<.compute> let rightResult = mixJob(take right)
  return try await (leftResult, rightResult)
}

export async fn mixBatch(
  jobs: take Array<MixingJob>,
  parallelism: usize,
): Array<MixingResult> throws BrigadeError {
  guard parallelism > 0 && parallelism <= maximumParallelCooks else {
    throw .invalidParallelism(found: parallelism, maximum: maximumParallelCooks)
  }

  return try await TaskGroup.parallelMap<.compute>(
    take jobs,
    limit: parallelism,
    ordering: .input,
    using: mixJob,
  )
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
