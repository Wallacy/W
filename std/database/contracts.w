// Typed descriptors and execution contracts for database adapters.
// Execution accepts descriptors only through const call parameters.

export enum Dialect {
  portable
  sqlite
  postgresql
}

export struct Binding {
  package name: String
  package dialect: Dialect

  export const init(name: String, dialect: Dialect) {
    self.name = name
    self.dialect = dialect
  }
}

export struct Query<Parameters, Row> {
  package text: String
  package dialect: Dialect

  export const init(
    text: String,
    dialect: Dialect = .portable,
  ) {
    self.text = text
    self.dialect = dialect
  }
}

export struct Command<Parameters> {
  package text: String
  package dialect: Dialect

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

export enum TransactionFailure<Failure: Error>: Error {
  operation(Failure)
  database(DatabaseError)
  unknownCommit(EffectId)
}

export protocol Transaction {
  async fn execute<Parameters>(
    command: const Command<Parameters>,
    parameters: Parameters,
  ): u64 throws DatabaseError

  async fn executeMany<Parameters>(
    command: const Command<Parameters>,
    parameters: take Array<Parameters>,
  ): u64 throws DatabaseError
}

export protocol Database {
  async fn one<Parameters, Row>(
    query: const Query<Parameters, Row>,
    parameters: Parameters,
  ): Row throws DatabaseError

  async fn optional<Parameters, Row>(
    query: const Query<Parameters, Row>,
    parameters: Parameters,
  ): Row? throws DatabaseError

  async fn all<Parameters, Row>(
    query: const Query<Parameters, Row>,
    parameters: Parameters,
    limits: RowLimits,
  ): Array<Row> throws DatabaseError

  async fn queryMany<Parameters, Row>(
    query: const Query<Parameters, Row>,
    parameters: take Array<Parameters>,
    maximumInFlight: usize<(1...)>,
  ): Array<Row> throws DatabaseError

  async fn transaction<Input, Output, Failure: Error>(
    input: take Input,
    isolation: Isolation,
    using operation: some async fn(
      ref any Transaction,
      take Input,
    ): Output throws Failure,
  ): Output throws TransactionFailure<Failure>
}
