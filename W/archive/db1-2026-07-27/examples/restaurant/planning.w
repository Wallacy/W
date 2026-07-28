// W Working Draft — pseudocódigo pedagógico, não executável.
// Scheduler guloso determinístico: lógica e mutação local, não wrappers async.

import { OrderId } from restaurant.domain
import { Temperature } from restaurant.units

export struct BakeJob {
  orderId: OrderId
  duration: Duration
  deadline: Instant
  temperature: Temperature
}

export struct ScheduledBake {
  orderId: OrderId
  lane: Int
  startsAt: Instant
  finishesAt: Instant
  temperature: Temperature
}

export struct BakeSchedule {
  entries: List<ScheduledBake>
  makespan: Duration
  lateJobs: Int
}

export enum PlanningError: Error {
  noLanes
  invalidDuration(OrderId)
}

fn earliestLane(loads: ref List<Duration>): Int {
  var best = 0
  for lane in 1..<loads.count {
    if loads[lane] < loads[best] {
      best = lane
    }
  }
  return best
}

fn maximum(loads: ref List<Duration>): Duration {
  var result = Duration.zero
  for load in loads {
    if load > result {
      result = load
    }
  }
  return result
}

// Earliest-deadline-first + lane menos carregada. O sort e as allocations
// pertencem à API de List e devem aparecer no cost lens.
export fn scheduleBakes(jobs: ref List<BakeJob>, laneCount: Int, opening: Instant): BakeSchedule throws PlanningError {
  guard laneCount > 0 else {
    throw .noLanes
  }

  var ordered = copy jobs
  ordered.sort(by: (left, right) => left.deadline < right.deadline)
  var loads = List.filled(count: laneCount, with: Duration.zero)
  var entries: List<ScheduledBake> = []
  var lateJobs = 0

  for job in ordered {
    guard job.duration > Duration.zero else {
      throw .invalidDuration(job.orderId)
    }

    let lane = earliestLane(loads)
    let startsAt = opening + loads[lane]
    let finishesAt = startsAt + job.duration
    if finishesAt > job.deadline {
      lateJobs += 1
    }

    entries.append(ScheduledBake(
      orderId: job.orderId,
      lane: lane,
      startsAt: startsAt,
      finishesAt: finishesAt,
      temperature: job.temperature,
    ))
    loads[lane] += job.duration
  }

  return BakeSchedule(entries: take entries, makespan: maximum(loads), lateJobs: lateJobs)
}
