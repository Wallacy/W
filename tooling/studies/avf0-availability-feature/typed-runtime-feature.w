// AVF0 source-shaped composition witness. It does not implement a provider.
module avf0_typed_runtime_feature

enum CheckoutPath {
  control
  candidate
}

struct CheckoutContext {
  tenant: String
  userId: String
}

struct FeatureDecision<Value> {
  value: Value
  generation: String
  configurationDigest: String
  freshness: String
}

fn selectCheckout(_ decision: FeatureDecision<CheckoutPath>): String {
  return switch decision.value {
    case .control: "stable"
    case .candidate: "candidate"
  }
}
