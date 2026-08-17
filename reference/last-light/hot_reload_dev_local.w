// HRD0 paired Last Light witness: local dev-runner projection.
// This is a parseable source fixture. It does not execute a reload.

module hot_reload_dev_local

import {
  DevRunnerEvent,
  DevRunnerOutcome,
  ReloadInput,
  ReloadResult,
} from hot_reload_dev_contract

export enum DevRunnerPhase {
  prepare
  validate
  preflight
  ready
  switch
  drain
  degraded
}

export struct LocalGenerationWitness {
  input: ReloadInput
  result: ReloadResult
}

// This pure projection records the same input/result frontier as the split witness.
export fn localOutcome(on event: DevRunnerEvent): DevRunnerOutcome {
  return switch event {
    case .switch: .committed
    case .staleCompletion: .staleRejected
    case .cleanup: .drained
    case .edit: .pending
    case .prepare: .pending
    case .validate: .pending
    case .preflight: .pending
    case .ready: .pending
    case .closeAdmission: .pending
    case .drain: .pending
  }
}
