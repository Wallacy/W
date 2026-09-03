// ABI laboratory for the horizon monitor.
//
// The exported body is W. The call contract and carriers use the target C ABI.
// No W owner, allocator, or hidden runtime context crosses this boundary.

import {
  HorizonStatus,
  classifyHorizon,
} from horizon

export foreign c {
  const LL_HORIZON_OK_V1: c.int = 0
  const LL_HORIZON_NON_FINITE_SCORE_V1: c.int = 1
  const LL_HORIZON_NEGATIVE_SCORE_V1: c.int = 2
  const LL_HORIZON_STABLE_V1: c.uint = 0
  const LL_HORIZON_WARNING_V1: c.uint = 1
  const LL_HORIZON_EVACUATION_V1: c.uint = 2

  /// Check error before reading kind. Score carries the input for diagnostics.
  struct ll_horizon_result_v1 {
    error: c.int
    kind: c.uint
    score: c.float
  }

  // This is a call-scoped input view. `data` is valid for `length` bytes only
  // during the call. The callee cannot retain it.
  struct ll_horizon_bytes_v1 {
    data: c.ptr<c.uchar>
    length: c.size
  }

  // Any API that returns this carrier must also publish its matching destroy
  // symbol. A C caller never sends `data` to its ambient `free`.
  struct ll_horizon_owned_bytes_v1 {
    context: c.ptr<c.void>
    data: c.ptr<c.uchar>
    length: c.size
    capacity: c.size
  }
}

// `data` is borrowed for this call. The façade exports no hidden owner or
// runtime context, and the C island cannot retain the pointer.
export unsafe fn<abi: .c> ll_horizon_checksum_v1(
  data: c.ptr<c.uchar>,
  size: c.size,
): c.uint {
  var hash: c.uint = 2_166_136_261
  var index: c.size = 0
  while index < size {
    hash = (hash ^ data[index]) * 16_777_619
    index += 1
  }
  return hash
}

export unsafe fn<abi: .c> ll_horizon_classify_v1(
  score: c.float,
): ll_horizon_result_v1 {
  do {
    let status = try classifyHorizon(score: score)
    let kind: c.uint = switch status {
      case .stable: LL_HORIZON_STABLE_V1
      case .warning(_): LL_HORIZON_WARNING_V1
      case .evacuation(_): LL_HORIZON_EVACUATION_V1
    }

    return ll_horizon_result_v1(error: LL_HORIZON_OK_V1, kind: kind, score: score)
  } catch .nonFinite {
    return ll_horizon_result_v1(
      error: LL_HORIZON_NON_FINITE_SCORE_V1,
      kind: LL_HORIZON_STABLE_V1,
      score: score,
    )
  } catch .invalidScore(_) {
    return ll_horizon_result_v1(
      error: LL_HORIZON_NEGATIVE_SCORE_V1,
      kind: LL_HORIZON_STABLE_V1,
      score: score,
    )
  }
}
