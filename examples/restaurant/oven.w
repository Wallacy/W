// W Working Draft — pseudocódigo pedagógico, não executável.
// Controle térmico mistura cálculo puro, estado local e I/O estruturado.

import { CakeFlavor } from restaurant.domain
import {
  Area,
  Power,
  Ratio,
  Temperature,
  TemperatureDelta,
  TemperatureRate,
  ThermalCapacity,
  ThermalTransmittance,
  absolute,
  absoluteRate,
  clampRatio,
} from restaurant.units

export struct ThermalModel {
  cavityCapacity: ThermalCapacity
  foodCapacity: ThermalCapacity
  surface: Area
  transmittance: ThermalTransmittance
  heaterPower: Power
  coupling: Power
}

export struct ThermalState {
  cavity: Temperature
  food: Temperature
  cavityRate: TemperatureRate
}

export struct HeatProfile {
  emptyRampRate: TemperatureRate
}

fn deriveHeatProfile(model: ref ThermalModel): HeatProfile {
  return HeatProfile(emptyRampRate: model.heaterPower / model.cavityCapacity)
}

// O behavior preserva `HeatProfile` como tipo lógico e só calcula no primeiro
// acesso. Acesso concorrente e failure precisam seguir o contrato de Lazy.
export object CalibratedOven {
  model: ThermalModel
  var Lazy heatProfile = deriveHeatProfile(model)
}

export struct ControllerConfig {
  proportional: f64
  integral: f64
  derivative: f64
  controlBand: TemperatureDelta
  stableBand: TemperatureDelta
  stableRate: TemperatureRate
}

export struct ControllerState {
  var accumulatedError: f64
  var previousError: f64
}

export struct ControlDecision {
  duty: Ratio
  predicted: ThermalState
  stable: Bool
}

export enum OvenControlError: Error {
  sensorUnavailable
  heaterUnavailable
  invalidStep
  nonFiniteControl
  timeout
}

export protocol OvenHardwareApi {
  async fn sample(): ThermalState throws OvenControlError
  async fn apply(duty: Ratio): Void throws OvenControlError
  async fn wait(during step: Duration): Void throws OvenControlError
}

/// Retorna o setpoint recomendado para um sabor de bolo.
///
/// ```w test
/// expect cakeSetpoint(.vanilla) == 180C
/// ```
export fn cakeSetpoint(flavor: CakeFlavor): Temperature {
  switch flavor {
    case .chocolate:
      return 178[degC]
    case .vanilla:
      return 180C
    case .carrot:
      return 347F
  }
}

export fn heatLoss(
  surface: Area,
  transmittance: ThermalTransmittance,
  inside: Temperature,
  ambient: Temperature,
): Power {
  return surface * transmittance * (inside - ambient)
}

// Balanço de energia em um passo fixo. Quantidades incompatíveis devem falhar
// no type checker e as unidades podem desaparecer no lowering.
export fn predictStep(
  model: ref ThermalModel,
  state: ThermalState,
  ambient: Temperature,
  duty: Ratio,
  elapsed: Duration,
): ThermalState {
  let wallLoss = heatLoss(model.surface, transmittance: model.transmittance, inside: state.cavity, ambient: ambient)
  let foodTransfer = model.coupling * (state.cavity - state.food) / 1[deltaK]
  let cavityEnergy = (model.heaterPower * duty - wallLoss - foodTransfer) * elapsed
  let foodEnergy = foodTransfer * elapsed
  let nextCavity = state.cavity + cavityEnergy / model.cavityCapacity
  let nextFood = state.food + foodEnergy / model.foodCapacity

  return ThermalState(cavity: nextCavity, food: nextFood, cavityRate: (nextCavity - state.cavity) / elapsed)
}

fn shouldAccumulate(rawDuty: f64, error: f64): Bool {
  switch rawDuty {
    case 0.0...1.0:
      return true
    case ..<0.0 where error > 0.0:
      return true
    case 1.0>.. where error < 0.0:
      return true
    case _:
      return false
  }
}

// PID normalizado com saturação e anti-windup. A função só muta o pequeno
// ControllerState; o modelo e o sample continuam values emprestados/copiados.
export fn controlStep(
  config: ref ControllerConfig,
  model: ref ThermalModel,
  sample: ThermalState,
  controller: inout ControllerState,
  target: Temperature,
  ambient: Temperature,
  elapsed: Duration,
): ControlDecision throws OvenControlError {
  guard elapsed > Duration.zero else {
    throw .invalidStep
  }

  let error = (target - sample.cavity) / config.controlBand
  let candidateIntegral = controller.accumulatedError + error * elapsed.seconds
  let derivative = (error - controller.previousError) / elapsed.seconds
  let rawDuty = config.proportional * error + config.integral * candidateIntegral + config.derivative * derivative
  guard rawDuty.isFinite else {
    throw .nonFiniteControl
  }
  let duty = clampRatio(rawDuty)

  // Só acumula quando o actuator não está saturado na direção do erro.
  if shouldAccumulate(rawDuty, error: error) {
    controller.accumulatedError = candidateIntegral
  }
  controller.previousError = error

  let predicted = predictStep(model, state: sample, ambient: ambient, duty: duty, elapsed: elapsed)
  let stable = absolute(target - predicted.cavity) <= config.stableBand
    && absoluteRate(predicted.cavityRate) <= config.stableRate
  return ControlDecision(duty: duty, predicted: predicted, stable: stable)
}

// A espera é concorrente, não paralelismo de CPU. Cada iteração tem pontos de
// suspensão, cancelamento e I/O visíveis; nenhum background task escapa.
export async fn regulateOven(
  hardware: ServiceRef<OvenHardwareApi>,
  model: ref ThermalModel,
  config: ref ControllerConfig,
  target: Temperature,
  ambient: Temperature,
  step: Duration,
  timeout: Duration,
): ThermalState throws OvenControlError {
  var controller = ControllerState(accumulatedError: 0.0, previousError: 0.0)
  var elapsed = Duration.zero

  while elapsed < timeout {
    let sample = try await hardware.sample()
    let decision = try controlStep(
      config,
      model: model,
      sample: sample,
      controller: inout controller,
      target: target,
      ambient: ambient,
      elapsed: step,
    )
    try await hardware.apply(decision.duty)
    if decision.stable {
      return decision.predicted
    }
    try await hardware.wait(during: step)
    elapsed += step
  }

  throw .timeout
}

test "setpoints usam pontos de temperatura equivalentes" for cakeSetpoint {
  expect cakeSetpoint(.chocolate) == 178[degC]
  expect cakeSetpoint(.vanilla) == 180[°C]
  expect cakeSetpoint(.carrot) == 347F
}
