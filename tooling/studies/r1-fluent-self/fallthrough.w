// R1 expression-core study variant.

struct OscillatorState {
  var phase: f32

  mut fn reset(): self {
    phase = 0.0
  }
}

test "self return uses the receiver reborrow on fallthrough" {
  var state = OscillatorState(phase: 0.75)
  let returned = state.reset()
  expect returned.phase == 0.0
}
