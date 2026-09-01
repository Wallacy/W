// Structured transaction oracle for one local or remote provider.

import { GuestId } from domain
import { TableId } from dining
import {
  Isolation,
  TransactionAccess,
} from std.database
import {
  TransactionFailure,
  Transactional,
} from std.runtime.transaction

export type ReservationId = u64

export enum BookingError: Error {
  unavailable(TableId)
  conflict(TableId)
  service(ServiceFailure)
}

export struct TableReservation {
  id: ReservationId
  tableId: TableId
  guestId: GuestId
}

export struct ReservationReceipt {
  reservationId: ReservationId
  tableId: TableId
}

export struct TableTransactionContract {
  isolation: Isolation
  access: TransactionAccess
}

export protocol TableTransaction {
  async fn reserve(
    tableId: TableId,
    guestId: GuestId,
  ): TableReservation throws BookingError

  async fn confirm(
    reservation: take TableReservation,
  ): ReservationReceipt throws BookingError
}

export protocol TableLedgerApi:
  Transactional<TableTransaction, TableTransactionContract, BookingError> {}

export async fn reserveTableAtomically(
  ledger: ref ServiceRef<TableLedgerApi>,
  tableId: TableId,
  guestId: GuestId,
): ReservationReceipt throws TransactionFailure<BookingError, BookingError> {
  return try await pipeline<transaction: {
    isolation: .serializable,
    access: .readWrite,
  }> tx = ledger {
    let reservation = try await tx.reserve(tableId: tableId, guestId: guestId)
    let receipt = try await tx.confirm(reservation: take reservation)
    commit receipt
  }
}

export enum TransactionObservation {
  bodyError
  canceledBeforeCommit
  commitConfirmed
  commitConfirmationLost
}

export enum ExpectedTransactionOutcome {
  aborted
  committed
  unknownCommit
}

export const fn expectedTransactionOutcome(
  observation: TransactionObservation,
): ExpectedTransactionOutcome {
  return switch observation {
    case .bodyError: .aborted
    case .canceledBeforeCommit: .aborted
    case .commitConfirmed: .committed
    case .commitConfirmationLost: .unknownCommit
  }
}

test "commit uncertainty never becomes rollback" for expectedTransactionOutcome {
  expect expectedTransactionOutcome(.bodyError) == .aborted
  expect expectedTransactionOutcome(.canceledBeforeCommit) == .aborted
  expect expectedTransactionOutcome(.commitConfirmed) == .committed
  expect expectedTransactionOutcome(.commitConfirmationLost) == .unknownCommit
}
