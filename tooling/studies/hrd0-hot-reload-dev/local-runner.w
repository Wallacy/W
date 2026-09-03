// HRD0 bundle fixture. This source-shaped witness does not execute a reload.

module hrd0_bundle_local

import { ReloadInput, ReloadResult } from hot_reload_dev_contract

export struct HotReloadDevFixture {
  let input: ReloadInput
  let result: ReloadResult
}
