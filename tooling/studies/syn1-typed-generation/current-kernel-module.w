import accelerator from std

export type ScoreKernel = fn(f32): f32

export const lastLightKernels = accelerator.module<{
  score: ScoreKernel,
}>()

fn exposeKernels(): String {
  return "kernel module"
}

entry(exposeKernels)
