// R1 expression-core semantic-negative variant.

struct OscillatorState {
  var phase: f32

  mut fn reset() {
    phase = 0.0
  }
}

test "omitted return type is Unit" {
  var state = OscillatorState(phase: 0.75)
  state.reset()
  expect state.phase == 0.0
}
