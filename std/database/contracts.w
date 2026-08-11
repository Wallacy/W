// Typed descriptors and execution contracts for database adapters.
// Execution accepts descriptors only through const call parameters.

import {
  TransactionFailure as RuntimeTransactionFailure,
  Transactional,
} from std.runtime.transaction

export enum Dialect {
  portable
  sqlite
  postgresql
}

export struct Binding {
  name: String
  dialect: Dialect

  export const init(name: String, dialect: Dialect) {
    self.name = name
    self.dialect = dialect
  }
}

export struct Query<Parameters, Row> {
  text: String
  dialect: Dialect

  export const init(
    text: String,
    dialect: Dialect = .portable,
  ) {
    self.text = text
    self.dialect = dialect
  }
}

export struct Command<Parameters> {
  text: String
  dialect: Dialect

  export const init(
    text: String,
    dialect: Dialect = .portable,
  ) {
    self.text = text
    self.dialect = dialect
  }
}

export struct RowLimits {
  rows: usize<(1...)>
  bytes: usize<(1...)>
}

export enum Isolation {
  readCommitted
  repeatableRead
  serializable
}

export enum TransactionAccess {
  readOnly
  readWrite
}

export struct TransactionContract {
  isolation: Isolation
  access: TransactionAccess
}

export enum DatabaseError: Error {
  unavailable
  overloaded
  timedOut
  unsupported
  invalidStatement(query: String)
  constraint(query: String)
  decode(query: String)
  missingRow(query: String)
  extraRows(query: String)
  resultLimit(query: String)
  protocol
}

export alias TransactionFailure<Failure: Error> =
  RuntimeTransactionFailure<Failure, DatabaseError>

export protocol Transaction {
  async fn one<Parameters, Row>(
    query: const Query<Parameters, Row>,
    parameters values: Parameters,
  ): Row throws DatabaseError

  async fn optional<Parameters, Row>(
    query: const Query<Parameters, Row>,
    parameters values: Parameters,
  ): Row? throws DatabaseError

  async fn all<Parameters, Row>(
    query: const Query<Parameters, Row>,
    parameters values: Parameters,
    limits rowLimits: RowLimits,
  ): Array<Row> throws DatabaseError

  async fn execute<Parameters>(
    command: const Command<Parameters>,
    parameters values: Parameters,
  ): u64 throws DatabaseError

  async fn executeMany<Parameters>(
    command: const Command<Parameters>,
    parameters values: take Array<Parameters>,
  ): u64 throws DatabaseError
}

export protocol Database:
  Transactional<Transaction, TransactionContract, DatabaseError> {
  async fn one<Parameters, Row>(
    query: const Query<Parameters, Row>,
    parameters values: Parameters,
  ): Row throws DatabaseError

  async fn optional<Parameters, Row>(
    query: const Query<Parameters, Row>,
    parameters values: Parameters,
  ): Row? throws DatabaseError

  async fn all<Parameters, Row>(
    query: const Query<Parameters, Row>,
    parameters values: Parameters,
    limits rowLimits: RowLimits,
  ): Array<Row> throws DatabaseError

  async fn queryMany<Parameters, Row>(
    query: const Query<Parameters, Row>,
    parameters values: take Array<Parameters>,
    maximumInFlight concurrency: usize<(1...)>,
  ): Array<Row> throws DatabaseError

}
