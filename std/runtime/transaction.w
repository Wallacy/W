// Shared contract for structured transactions.

export enum TransactionFailure<OperationFailure: Error, ProviderFailure: Error>: Error {
  operation(OperationFailure)
  provider(ProviderFailure)
  unknownCommit(EffectId)
}

export protocol Transactional<Scope, Contract, Failure: Error> {
  async fn runTransaction<Output, OperationFailure: Error>(
    contract: const Contract,
    using operation: some async fn(ref Scope): Output throws OperationFailure,
  ): Output throws TransactionFailure<OperationFailure, Failure>
}

test "transaction failure keeps an unknown commit distinct" {
  let outcome: TransactionFailure<Never, Never> = .unknownCommit(EffectId(42))

  expect switch outcome {
    case .operation(_): false
    case .provider(_): false
    case .unknownCommit(let id): id == EffectId(42)
  }
}
