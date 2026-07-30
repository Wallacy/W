// Device entry for the restaurant sound controller.

import {
  AudioRenderState,
  DeviceAudioBlock,
  renderFinalSong,
} from restaurant.audio

fn render(
  block: inout DeviceAudioBlock,
  state: inout AudioRenderState,
  ctx: AudioRenderContext,
): AudioRenderResult {
  return renderFinalSong(inout state, block: inout block, ctx: ctx)
}

entry LastLightAudio(render)
