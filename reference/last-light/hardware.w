// C boundary for the Sonda de Aroma.

import { Probability } from domain
import { Temperature } from units
import si from std

foreign c from "last_light_probe.h" {
  type ll_probe

  struct ll_sample {
    aroma: c.double
    kelvin: c.double
    status: c.int
  }

  fn ll_probe_read(_ probe: c.ptr<ll_probe>, _ sample: c.ptr<ll_sample>): c.int
  fn ll_probe_close(_ probe: c.ptr<ll_probe>)
}

// Binding metadata classifies ll_probe_read as blocking. The device adapter
// runs it outside service and cooperative executor threads.

export enum ProbeError: Error {
  readFailed(status: c.int)
  nonFinite
  invalidAroma(RefinementError)
}

export struct ProbeSample {
  let aroma: Probability
  let temperature: Temperature
}

export protocol AromaProbeApi {
  async fn sample(): ProbeSample throws ProbeError
}

export object AromaProbeDevice {
  let handle: c.ptr<ll_probe>

  init(handle: c.ptr<ll_probe>) {
    self.handle = handle
  }

  deinit {
    unsafe { ll_probe_close(handle) }
  }
}

// W-1233: inline body; the builder owns the reproducible foreign-unit grouping.
unsafe fn<C> legacyProbeStatus(status: c.int): c.int {
  const char *closing_brace_note = "}";
  /* This brace belongs to a C comment, not to the W module: } */
  if (status < 0) {
    return status;
  }
  return closing_brace_note[0] == '}' ? status : -1;
}

export service aromaProbe: AromaProbeApi {
  let device: AromaProbeDevice

  init(device: AromaProbeDevice) {
    self.device = device
  }

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
