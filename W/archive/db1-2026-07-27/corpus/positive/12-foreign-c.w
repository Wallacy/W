foreign c from "math.h" {
  fn cos(_ value: c.double): c.double
}

fn cosine(value: f64): f64 {
  return cos(value)
}
