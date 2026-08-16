package {
  schema: "w.package/1"
  authority: .registry("w")
  name: "last-light/availability-study"
  version: "0.1.0"

  moduleSets: [
    {
      name: "checkout-candidate"
      activation: .inactive
      namespace: "checkout"
      root: "candidate"
      include: ["*.w"]
      layout: .fileStem
    },
  ]

  features: [
    {
      name: "candidate-checkout"
      enables: [.moduleSet("checkout-candidate")]
    },
  ]

  products: [
    {
      name: "availability-study"
      kind: .executable
      features: ["candidate-checkout"]
    },
  ]
}
