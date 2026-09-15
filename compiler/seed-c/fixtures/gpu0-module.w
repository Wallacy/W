module gpu0<kernels: { hello: helloKernel }>

fn helloKernel(): i32 {
  return 42
}

entry { }
