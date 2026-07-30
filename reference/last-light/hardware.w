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

// Binding metadata classifies ll_probe_read as blocking. The device adapter
// runs it outside service and cooperative executor threads.

export enum ProbeError: Error {
  readFailed(status: c.int)
  nonFinite
  invalidAroma(RefinementError)
}

export struct ProbeSample {
  aroma: Probability
  temperature: Temperature
}

export protocol AromaProbeApi {
  async fn sample(): ProbeSample throws ProbeError
}

export object AromaProbeDevice {
  package handle: c.ptr<ll_probe>

  package init(handle: c.ptr<ll_probe>) {
    self.handle = handle
  }

  deinit {
    unsafe { ll_probe_close(handle) }
  }
}

unsafe fn<C> legacyProbeStatus(status: c.int): c.int {
  return status;
}

export service AromaProbeService as AromaProbeApi {
  device: AromaProbeDevice

  mut async fn sample(): ProbeSample throws ProbeError {
    var raw: ll_sample
    let status = unsafe {
      legacyProbeStatus(ll_probe_read(device.handle, inout raw))
    }
    guard status == 0 else throw .readFailed(status: status)
    guard raw.aroma.isFinite && raw.kelvin.isFinite else throw .nonFinite

    return ProbeSample(
      aroma: try Probability(raw.aroma),
      temperature: Quantity(raw.kelvin, unit: si.K),
    )
  }
}
