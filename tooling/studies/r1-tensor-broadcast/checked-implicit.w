// R1 Last Light tensor-broadcast study variant.

fn centerSamples<samples: usize>(
  calibrated: ref Tensor<f32, shape: [samples, 6]>,
  means: ref Tensor<f32, shape: [6]>,
): Tensor<f32, shape: [samples, 6]> {
  return calibrated - means
}

test "checked implicit broadcast keeps the sample shape" for centerSamples {
  let centered = centerSamples<3>(calibrated, means)
  expect centered.shape == [3, 6]
}
