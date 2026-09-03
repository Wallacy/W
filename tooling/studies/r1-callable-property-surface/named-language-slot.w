module callable_property_surface
import { Arrival, Welcome } from callables

unsafe fn<lang: .c> legacyProbeStatus(status: c.int): c.int {
  return status;
}

fn callableSurfaceEntry(_ arrival: Arrival, _ gate: usize): Welcome {
  return Welcome(orderId: arrival.orderId, gate: gate)
}
