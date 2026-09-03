module callable_property_surface
import { Arrival, Welcome } from callables

struct PidController {
  accumulatedError: f64
  previousError: f64

  export isIdle: Bool {
    get => accumulatedError == 0.0 && previousError == 0.0
  }
}

unsafe fn<C> legacyProbeStatus(status: c.int): c.int {
  return status;
}

fn callableSurfaceEntry(_ arrival: Arrival, _ gate: usize): Welcome {
  let greet = <[copy gate]> (arrival) => Welcome(orderId: arrival.orderId, gate: gate)
  return greet(arrival)
}
