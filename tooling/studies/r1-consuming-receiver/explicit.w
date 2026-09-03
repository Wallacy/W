// R1 Last Light consuming-receiver study source.

enum CommandError: Error {
  invalidTail
}

object CommandStream {
  var tail = String()

  init(tail: take String) {
    self.tail = take tail
  }

  take fn finish(): usize throws CommandError {
    if tail == "invalid" {
      throw .invalidTail
    }

    return tail.bytes.count
  }
}

enum FinishObservation {
  completed(bytes: usize, ownerAvailable: Bool)
  failed(error: CommandError, ownerAvailable: Bool)
}

fn observeFinish(_ tail: take String): FinishObservation {
  let cursor = CommandStream(tail: take tail)

  do {
    let bytes = try (take cursor).finish()
    return .completed(bytes: bytes, ownerAvailable: false)
  } catch .invalidTail {
    return .failed(error: .invalidTail, ownerAvailable: false)
  }
}

test "finishing consumes the command stream in every outcome" for observeFinish {
  expect observeFinish("cake") == .completed(bytes: 4, ownerAvailable: false)
  expect observeFinish("invalid") == .failed(error: .invalidTail, ownerAvailable: false)
}
