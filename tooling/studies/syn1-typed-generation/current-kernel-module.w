module current_kernel_module<
  kernels: { score: scoreKernel },
>

fn scoreKernel(value: f32): f32 {
  return value
}

fn exposeKernels(): String {
  return "kernel module"
}

entry(exposeKernels)
