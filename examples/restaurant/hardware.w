// C boundary for the Sonda de Aroma.

import { Probability } from restaurant.domain
import { Temperature } from restaurant.units
import std.si

foreign c from "last_light_probe.h" {
  type ll_probe

  struct ll_sample {
    aroma: c.double
    kelvin: c.double
    status: c.int
  }

  fn ll_probe_read(probe: c.ptr<ll_probe>, sample: c.ptr<ll_sample>): c.int
  fn ll_probe_close(probe: c.ptr<ll_probe>)
}

export enum ProbeError: Error {
  closed
  readFailed(status: c.int)
  nonFinite
  invalidAroma(RefinementError)
}

export struct ProbeSample {
  aroma: Probability
  temperature: Temperature
}

export object AromaProbe {
  handle: c.ptr<ll_probe>?

  deinit {
    if let handle = handle {
      unsafe { ll_probe_close(handle) }
    }
  }

  mut fn read(): ProbeSample throws ProbeError {
    guard let handle = handle else throw .closed

    var raw: ll_sample
    let status = unsafe { ll_probe_read(handle, inout raw) }
    guard status == 0 else throw .readFailed(status: status)
    guard raw.aroma.isFinite && raw.kelvin.isFinite else throw .nonFinite

    return ProbeSample(
      aroma: try Probability(raw.aroma),
      temperature: Quantity(raw.kelvin, unit: si.K),
    )
  }
}
