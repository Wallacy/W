// Bare-metal lifecycle for horizon sensors and satellite links.

import std.device as device
import { HorizonSample } from restaurant.horizon

struct ControllerState {
  var atomic latestSequence: u64
}

fn reset(state: inout ControllerState, ctx: device.Context): () {
  state.latestSequence.store<.relaxed>(0)
  ctx.memory.initialize()
  ctx.interrupts.enable()
}

package fn sampleTick(state: inout ControllerState, ctx: device.Context): () {
  let sample: HorizonSample = ctx.sensors.readHorizon()
  state.latestSequence.store<.release>(sample.sequence)
  let _ = ctx.telemetry.trySend(sample)
}

package fn interrupt(
  event: device.InterruptEvent,
  state: inout ControllerState,
  ctx: device.Context,
): () {
  switch event.line {
    case .sensorTimer:
      sampleTick(inout state, ctx: ctx)
    case .radio:
      ctx.radio.drain()
    case _:
      ctx.interrupts.mask(event.line)
  }
}

entry LastLightController(reset)
