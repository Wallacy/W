// Bare-metal lifecycle for horizon sensors and satellite links.

import { HorizonSample } from restaurant.horizon

struct ControllerState {
  var atomic latestSequence: u64
}

fn reset(state: inout ControllerState, ctx: DeviceContext): () {
  state.latestSequence.store<.relaxed>(0)
  ctx.memory.initialize()
  ctx.interrupts.enable()
}

fn sampleTick(state: inout ControllerState, ctx: DeviceContext): () {
  let sample: HorizonSample = ctx.sensors.readHorizon()
  state.latestSequence.store<.release>(sample.sequence)
  let _ = ctx.telemetry.trySend(sample)
}

fn interrupt(
  event: InterruptEvent,
  state: inout ControllerState,
  ctx: DeviceContext,
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

entry LastLightController(reset) {
  device.tick = sampleTick
  device.interrupt = interrupt
}
