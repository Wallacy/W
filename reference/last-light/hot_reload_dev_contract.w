// HRD0 common nominal contract for paired development-runner witnesses.
// This source-shaped fixture records identity and event/result types only.

module hot_reload_dev_contract

export enum DevRunnerEvent {
  edit
  prepare
  validate
  preflight
  ready
  switch
  closeAdmission
  drain
  staleCompletion
  cleanup
}

export enum DevRunnerOutcome {
  pending
  committed
  staleRejected
  drained
}

export struct GenerationIdentity {
  let packageIdentity: String
  let semanticInterfaceKey: String
  let sourceMapKey: String
  let wAbiKey: String
  let runtimeClosureKey: String
  let schemaDigest: String
  let effectDigest: String
  let capabilityDigest: String
}

export struct ReloadInput {
  let sourceDigest: String
  let generation: String
  let identity: GenerationIdentity
}

export struct ReloadResult {
  let generation: String
  let outcome: DevRunnerOutcome
  let staleRejected: Bool
  let identity: GenerationIdentity
}
