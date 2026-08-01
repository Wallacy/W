// Device entry for the restaurant sound controller.

import std.audio as audio
import {
  AudioRenderState,
  DeviceAudioBlock,
  renderFinalSong,
} from restaurant.audio

fn render(
  block: inout DeviceAudioBlock,
  state: inout AudioRenderState,
  ctx: audio.RenderContext,
): audio.RenderResult {
  return renderFinalSong(inout state, block: inout block, ctx: ctx)
}

entry LastLightAudio(render)
