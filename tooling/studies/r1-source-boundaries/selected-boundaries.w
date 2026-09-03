module source_boundaries
import kitchen
import { ServiceStage } from domain

enum TraceStage {
  ready
}

fn trace(_ stage: TraceStage) {
  let observed = stage
  observed
}

fn sourceBoundaryEntry(): Int {
  return
  42
}

fn discardBoundary(_ ready: Bool): ServiceStage {
  return if ready {
    trace(.ready);
    .accepted
  } else {
    .reserving
  }
}

fn formatterBoundary(_ items: Array<String>): Array<String> {
  return items
}
