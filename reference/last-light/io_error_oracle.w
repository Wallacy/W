// Application-level recovery policy over the portable physical I/O error.

import * from std.io

export enum RecipeIoRecovery {
  stop
  retryAfterBackoff
  requireCallerDecision
}

export fn recipeIoRecovery(
  error: ref IoError,
  committedBytes: usize,
  idempotent: Bool,
  deadlineRemaining: Bool,
): RecipeIoRecovery {
  if committedBytes != 0 || !idempotent || !deadlineRemaining {
    return .requireCallerDecision
  }

  if error.operation.one(.open, .read)
    && error.kind.one(.busy, .timedOut) {
    return .retryAfterBackoff
  }

  return .stop
}

test "portable I/O error axes remain independent" {
  expect IoOperation.open != .read
  expect IoErrorKind.busy != .timedOut
  expect IoErrorKind.other != .invalidData
}

// Compile-fail assays:
// if error.retryable { retry() }       // No universal retry Boolean.
// let code = error.cause.nativeCode()  // A target module needs target proof.
// let wire = encode(error.cause)        // IoCause is not serializable.
