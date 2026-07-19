// W Working Draft — pseudocódigo pedagógico, não executável.
// A câmara fria força unidades, fórmulas e controle sem depender de framework.

import {
  Area,
  FlowResistance,
  MassFlow,
  Power,
  Pressure,
  Ratio,
  Temperature,
  TemperatureDelta,
  ThermalTransmittance,
  clampRatio,
} from restaurant.units

export struct RefrigerationModel {
  envelopeArea: Area
  transmittance: ThermalTransmittance
  fanPower: Power
  compressorPower: Power
  lineResistance: FlowResistance
}

export struct RefrigerationSample {
  chamber: Temperature
  ambient: Temperature
  suctionPressure: Pressure
  massFlow: MassFlow
}

export struct RefrigerationDecision {
  compressorDuty: Ratio
  expectedLoad: Power
  expectedPressureDrop: Pressure
}

export enum RefrigerationError: Error {
  invalidTemperatures
  pressureOutOfRange(Pressure)
  sensorUnavailable
}

export protocol RefrigerationApi {
  async fn sample(): RefrigerationSample throws RefrigerationError
  async fn apply(compressorDuty: Ratio): Void throws RefrigerationError
}

export fn conductiveLoad(model: ref RefrigerationModel, sample: ref RefrigerationSample): Power {
  return model.envelopeArea * model.transmittance * (sample.ambient - sample.chamber)
}

export fn pressureDrop(resistance: FlowResistance, flow: MassFlow): Pressure {
  return resistance * flow * flow
}

// COP de Carnot é uma referência ideal, não uma promessa sobre o equipamento.
export fn idealCoefficientOfPerformance(cold: Temperature, hot: Temperature): f64 throws RefrigerationError {
  guard hot > cold else {
    throw .invalidTemperatures
  }
  return cold.baseUnits / (hot - cold).baseUnits
}

export fn chooseRefrigerationDuty(
  model: ref RefrigerationModel,
  sample: ref RefrigerationSample,
  target: Temperature,
  controlBand: TemperatureDelta,
): RefrigerationDecision throws RefrigerationError {
  guard sample.suctionPressure > 0[Pa] else {
    throw .pressureOutOfRange(sample.suctionPressure)
  }

  let thermalLoad = conductiveLoad(model, sample: sample)
  let normalizedError = (sample.chamber - target) / controlBand
  let duty = clampRatio(normalizedError + thermalLoad / model.compressorPower)
  return RefrigerationDecision(
    compressorDuty: duty,
    expectedLoad: thermalLoad + model.fanPower,
    expectedPressureDrop: pressureDrop(model.lineResistance, flow: sample.massFlow),
  )
}
