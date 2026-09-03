// ATOM1 Research candidate. Existing source shape, compiler-derived atomicValue.

module atom1_derived_record

import atomic from std

export enum SignState {
  dark
  announcing
  closed
}

export struct SignEpochWord: Duplicable {
  let state: SignState
  let generation: u32
}

export object DerivedSignEpoch {
  // Candidate only. Current W rejects this payload fact in Atomic<T>.
  var atomic word: SignEpochWord = SignEpochWord(
    state: .dark,
    generation: 0,
  )

  export init() {}

  fn observe(): SignEpochWord {
    return word.load<.acquire>()
  }

  fn publish(_ next: SignEpochWord) {
    word.store<.release>(next)
  }

  fn exchange(_ next: SignEpochWord): SignEpochWord {
    return word.exchange<.acquireRelease>(next)
  }

  fn close(_ expected: SignEpochWord, _ desired: SignEpochWord): AtomicExchange<SignEpochWord> {
    return word.compareExchange<
      success: .acquireRelease,
      failure: .acquire,
    >(
      expected: expected,
      desired: desired,
    )
  }
}

// Generic fetch arithmetic is intentionally absent for a record.
