// Bare-metal lifecycle for horizon sensors and satellite links.

import device from std
import { HorizonSample } from horizon

struct ControllerState {
  var atomic latestSequence: u64
}

fn reset(state: inout ControllerState, ctx: device.Context): () {
  state.latestSequence.store<.relaxed>(0)
  ctx.memory.initialize()
  ctx.interrupts.enable()
}

fn sampleTick(state: inout ControllerState, ctx context: device.Context): () {
  let sample: HorizonSample = context.sensors.readHorizon()
  state.latestSequence.store<.release>(sample.sequence)
  let _ = context.telemetry.trySend(sample)
}

// W-1235: the firmware host slot fixes this handler's effects and budget.
fn interrupt(
  event: device.InterruptEvent,
  state: inout ControllerState,
  ctx: device.Context,
): () {
  switch event.line {
    case .sensorTimer:
      sampleTick(state: inout state, ctx: ctx)
    case .radio:
      ctx.radio.drain()
    case _:
      ctx.interrupts.mask(event.line)
  }
}

entry LastLightController(reset)
