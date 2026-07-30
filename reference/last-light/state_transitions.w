/// Runtime state and typestate solve different transition problems.
import { OrderId, ServiceStage, StagePath } from restaurant.domain
import { OvenId } from restaurant.kitchen

export alias StandardStagePath =
  StagePath<[.accepted, .reserving, .preparing, .serving, .completed]>

export alias CancelledStagePath =
  StagePath<[.accepted, .reserving, .cancelled]>

export enum OvenSessionState {
  idle
  ready
  faulted
  closed
}

export enum OvenSessionFault: Error {
  sensorUnavailable
}

export struct OvenSession<const _ state: OvenSessionState> {
  id: OvenId

  init(id: OvenId) {
    self.id = id
  }
}

export enum ActivationOutcome {
  ready(OvenSession<.ready>)
  faulted(OvenSession<.faulted>, OvenSessionFault)
}

export enum AnyOvenSession {
  idle(OvenSession<.idle>)
  ready(OvenSession<.ready>)
  faulted(OvenSession<.faulted>)
  closed(OvenSession<.closed>)
}

export fn openOvenSession(id: OvenId): OvenSession<.idle> {
  return OvenSession<.idle>(id: id)
}

extension OvenSession<.idle> {
  export take fn activate(sensorWorks: Bool): ActivationOutcome {
    if sensorWorks {
      return .ready(OvenSession<.ready>(id: id))
    }

    return .faulted(
      OvenSession<.faulted>(id: id),
      .sensorUnavailable,
    )
  }
}

extension OvenSession<.ready> {
  export take fn close(): OvenSession<.closed> {
    return OvenSession<.closed>(id: id)
  }
}

extension OvenSession<.faulted> {
  export take fn quarantine(): OvenSession<.closed> {
    return OvenSession<.closed>(id: id)
  }
}

extension OvenSession<.closed> {
  export take fn finish(): OvenId {
    return id
  }
}

export fn activationMessage(outcome: take ActivationOutcome): String {
  return switch outcome {
    case .ready(let oven):
      let closed = (take oven).close()
      let id = (take closed).finish()
      "Oven ${id} was ready and closed"
    case .faulted(let oven, let fault):
      let detail = switch fault {
        case .sensorUnavailable: "sensor unavailable"
      }
      let closed = (take oven).quarantine()
      let id = (take closed).finish()
      "Oven ${id} was quarantined: ${detail}"
  }
}

export type StageRevision = u64

export struct StageSnapshot {
  orderId: OrderId
  stage: ServiceStage
  revision: StageRevision
}

export enum MoveOrderResult {
  applied(StageSnapshot)
  stale(current: StageSnapshot)
  rejected(from: ServiceStage, to: ServiceStage)
}

export alias AppliedOrStale =
  MoveOrderResult<[.applied, .stale]>

export fn describeObservedMove(result: AppliedOrStale): String {
  return switch result {
    case .applied(let snapshot):
      "Order ${snapshot.orderId} moved at revision ${snapshot.revision}"
    case .stale(let snapshot):
      "Order ${snapshot.orderId} is already at revision ${snapshot.revision}"
  }
}

test "typestate consumes the previous oven session" for activationMessage {
  let ready = openOvenSession(7)
  let faulted = openOvenSession(8)

  expect activationMessage((take ready).activate(sensorWorks: true)) ==
    "Oven 7 was ready and closed"
  expect activationMessage((take faulted).activate(sensorWorks: false)) ==
    "Oven 8 was quarantined: sensor unavailable"
}

test "runtime snapshot keeps a revision" for describeObservedMove {
  let snapshot = StageSnapshot(orderId: 42, stage: .preparing, revision: 3)
  let result: AppliedOrStale = .applied(snapshot)

  expect describeObservedMove(result) ==
    "Order 42 moved at revision 3"
}
