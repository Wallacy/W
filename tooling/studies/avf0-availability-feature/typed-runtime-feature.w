// AVF0 source-shaped composition witness. It does not implement a provider.
module avf0_typed_runtime_feature

enum CheckoutPath {
  control
  candidate
}

struct CheckoutContext {
  let tenant: String
  let userId: String
}

struct FeatureDecision<Value> {
  let value: Value
  let generation: String
  let configurationDigest: String
  let freshness: String
}

fn selectCheckout(_ decision: FeatureDecision<CheckoutPath>): String {
  return switch decision.value {
    case .control: "stable"
    case .candidate: "candidate"
  }
}
