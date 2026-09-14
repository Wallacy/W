fn helloKernel(): i32 {
  return 42
}

export const kernels = accelerator.module<{
  hello: helloKernel
}>()

entry { }
