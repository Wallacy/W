// Device entry for the restaurant sound controller.

import audio from std
import {
  AudioRenderState,
  DeviceAudioBlock,
  renderFinalSong,
} from audio

fn render(
  block: inout DeviceAudioBlock,
  state: inout AudioRenderState,
  ctx: audio.RenderContext,
): audio.RenderResult {
  return renderFinalSong(state: inout state, block: inout block, ctx: ctx)
}

entry LastLightAudio(render)
