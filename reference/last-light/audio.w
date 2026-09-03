// Allocation-free audio rendering for the final song.

import audio from std
import math from std

export const audioFrames = 256_usize
export const audioChannels = 2_usize
export const audioSampleRate = 48_000_u32

export struct AudioBlock<frames: usize, channels: usize> {
  var samples: [f32; frames * channels]

  mut fn clear(): self {
    for index in 0..<samples.count {
      samples[index] = 0.0
    }
  }
}

export alias DeviceAudioBlock = AudioBlock<
  frames: audioFrames,
  channels: audioChannels,
>

export struct OscillatorState {
  var phase: f32
  let frequency: f32
  let gain: f32<(0.0...1.0)>
}

export enum AudioRenderError: Error {
  invalidSampleRate
  deadlineMissed
}

export fn renderTone(
  state: inout OscillatorState,
  block: inout DeviceAudioBlock,
  sampleRate: u32,
): () throws AudioRenderError {
  guard sampleRate > 0 else throw .invalidSampleRate

  let phaseStep = state.frequency / sampleRate.toF32()
  for frame in 0..<audioFrames {
    let sample = math.sin(state.phase * 2.0 * f32.pi) * state.gain
    state.phase = (state.phase + phaseStep).fraction

    for channel in 0..<audioChannels {
      block.samples[frame * audioChannels + channel] = sample
    }
  }
}

export struct AudioRenderState {
  var oscillator: OscillatorState
  var renderedFrames: u64
}

export fn renderFinalSong(
  state: inout AudioRenderState,
  block output: inout DeviceAudioBlock,
  ctx context: audio.RenderContext,
): audio.RenderResult {
  do {
    try renderTone(
      state: inout state.oscillator,
      block: inout output,
      sampleRate: context.sampleRate,
    )
    state.renderedFrames += audioFrames
    return .complete
  } catch error {
    output.clear()
    return .silence(reason: error)
  }
}

test "rendering writes one complete device block" for renderFinalSong {
  var state = AudioRenderState(
    oscillator: OscillatorState(phase: 0.0, frequency: 440.0, gain: 0.25),
    renderedFrames: 0,
  )
  var block = DeviceAudioBlock(samples: [0.0; audioFrames * audioChannels])
  let result = renderFinalSong(state: inout state, block: inout block, ctx: .test)
  expect result == .complete
  expect state.renderedFrames == audioFrames
}
