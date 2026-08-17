// ATOM2 selected contract witness. The carrier is compiler-synthesized.

module atom2_canonical_carrier

import atomic from std

export enum SignState {
  dark
  announcing
  closed
}

export struct SignEpochWord: Duplicable {
  state: SignState
  generation: u32
}

// Canonical value facts: state ordinal uses bits 0..1, generation uses bits 2..33.
// Unused high bits are zero; physical endian belongs to the provider/ABI.

export object CanonicalCarrier {
  var atomic word: SignEpochWord = SignEpochWord(state: .dark, generation: 0)

  export init() {}

  fn observe(): SignEpochWord {
    return word.load<.acquire>()
  }

  fn publish(_ next: SignEpochWord) {
    word.store<.release>(next)
  }

  fn close(expected: SignEpochWord, desired: SignEpochWord): AtomicExchange<SignEpochWord> {
    // expected and desired use the same canonical encoder before CAS comparison.
    return word.compareExchange<success: .acquireRelease, failure: .acquire>(expected: expected, desired: desired)
  }
}
