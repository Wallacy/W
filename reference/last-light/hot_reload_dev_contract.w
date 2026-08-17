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
  packageIdentity: String
  semanticInterfaceKey: String
  sourceMapKey: String
  wAbiKey: String
  runtimeClosureKey: String
  schemaDigest: String
  effectDigest: String
  capabilityDigest: String
}

export struct ReloadInput {
  sourceDigest: String
  generation: String
  identity: GenerationIdentity
}

export struct ReloadResult {
  generation: String
  outcome: DevRunnerOutcome
  staleRejected: Bool
  identity: GenerationIdentity
}
