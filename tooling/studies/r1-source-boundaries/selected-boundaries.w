module source_boundaries
import kitchen
import { ServiceStage } from domain

enum TraceStage {
  ready
}

fn trace(stage: TraceStage) {
  let observed = stage
  observed
}

fn sourceBoundaryEntry(): Int {
  return
  42
}

fn discardBoundary(ready: Bool): ServiceStage {
  return if ready {
    trace(.ready);
    .accepted
  } else {
    .reserving
  }
}

fn formatterBoundary(items: Array<String>): Array<String> {
  return items
}
