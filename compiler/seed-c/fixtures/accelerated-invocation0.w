fn kernel(): i32 {
  return 42
}

export const kernels = accelerator.module<{
  hello: kernel
}>()

entry {
  let pending = spawn<.inference> kernels.hello()
  let result = await pending
}
