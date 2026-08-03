// Pure oracle for logical execution-domain selection and bounded profile admission.

enum DomainName {
  unselected
  main
  compute
  io
  network
  thermal
}

enum DomainIntent {
  asyncTask
  parallelTask
}

enum DomainResolution {
  selected(DomainName)
  missingParallelDefault
}

const fn resolveDomain(
  intent: DomainIntent,
  explicit: DomainName,
  inherited: DomainName,
  parallelDefault: DomainName,
): DomainResolution {
  if explicit != .unselected {
    return .selected(explicit)
  }

  if inherited != .unselected {
    return .selected(inherited)
  }

  if intent == .parallelTask {
    if parallelDefault == .unselected {
      return .missingParallelDefault
    }

    return .selected(parallelDefault)
  }

  return .selected(.main)
}

struct DomainContract {
  name: DomainName
  parallel: Bool
  capacity: u16
}

enum AdmissionDecision {
  accepted
  serialDomain
  emptyCapacity
}

const fn admit(
  intent: DomainIntent,
  contract: DomainContract,
): AdmissionDecision {
  if contract.capacity == 0 {
    return .emptyCapacity
  }

  if intent == .parallelTask && !contract.parallel {
    return .serialDomain
  }

  return .accepted
}

enum CapacityDecision {
  reduced(u16)
  reject
}

const fn reduceCapacity(
  artifactMaximum: u16,
  deploymentMaximum: u16,
): CapacityDecision {
  if deploymentMaximum == 0 || deploymentMaximum > artifactMaximum {
    return .reject
  }

  return .reduced(deploymentMaximum)
}

test "domain selection is explicit, inherited, or profile default" for resolveDomain {
  expect resolveDomain(
    intent: .asyncTask,
    explicit: .thermal,
    inherited: .compute,
    parallelDefault: .io,
  ) == .selected(.thermal)

  expect resolveDomain(
    intent: .asyncTask,
    explicit: .unselected,
    inherited: .compute,
    parallelDefault: .io,
  ) == .selected(.compute)

  expect resolveDomain(
    intent: .parallelTask,
    explicit: .unselected,
    inherited: .unselected,
    parallelDefault: .compute,
  ) == .selected(.compute)

  expect resolveDomain(
    intent: .parallelTask,
    explicit: .unselected,
    inherited: .unselected,
    parallelDefault: .unselected,
  ) == .missingParallelDefault

  expect resolveDomain(
    intent: .asyncTask,
    explicit: .unselected,
    inherited: .unselected,
    parallelDefault: .unselected,
  ) == .selected(.main)
}

test "parallel intent rejects serial domains but accepts capacity one" for admit {
  expect admit(
    intent: .parallelTask,
    contract: DomainContract(name: .thermal, parallel: false, capacity: 1),
  ) == .serialDomain

  expect admit(
    intent: .parallelTask,
    contract: DomainContract(name: .compute, parallel: true, capacity: 1),
  ) == .accepted

  expect admit(
    intent: .asyncTask,
    contract: DomainContract(name: .main, parallel: false, capacity: 1),
  ) == .accepted

  expect admit(
    intent: .parallelTask,
    contract: DomainContract(name: .compute, parallel: true, capacity: 0),
  ) == .emptyCapacity
}

test "deployment can reduce but cannot increase profile capacity" for reduceCapacity {
  expect reduceCapacity(artifactMaximum: 8, deploymentMaximum: 4) == .reduced(4)
  expect reduceCapacity(artifactMaximum: 8, deploymentMaximum: 8) == .reduced(8)
  expect reduceCapacity(artifactMaximum: 8, deploymentMaximum: 9) == .reject
  expect reduceCapacity(artifactMaximum: 8, deploymentMaximum: 0) == .reject
}
