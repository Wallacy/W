module accelerated_invocation<kernels: { hello: kernel }>

fn kernel(): i32 {
  return 42
}

entry {
  let pending = spawn<.inference> hello()
  let result = await pending
}
