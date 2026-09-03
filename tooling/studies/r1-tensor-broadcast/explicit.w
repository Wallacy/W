// R1 Last Light tensor-broadcast study variant.

fn centerSamples<samples: usize>(
  _ calibrated: ref Tensor<f32, shape: [samples, 6]>,
  _ means: ref Tensor<f32, shape: [6]>,
): Tensor<f32, shape: [samples, 6]> {
  return calibrated - means.broadcast(to: [samples, 6])
}

test "explicit broadcast keeps the sample shape" for centerSamples {
  let centered = centerSamples<3>(calibrated, means)
  expect centered.shape == [3, 6]
}
