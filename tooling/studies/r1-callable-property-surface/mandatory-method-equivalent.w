module callable_property_surface
import { Arrival, Welcome } from callables

struct PidController {
  accumulatedError: f64
  previousError: f64

  fn isIdle(): Bool {
    return accumulatedError == 0.0 && previousError == 0.0
  }
}

fn callableSurfaceEntry(_ arrival: Arrival, _ gate: usize): Welcome {
  return Welcome(orderId: arrival.orderId, gate: gate)
}
