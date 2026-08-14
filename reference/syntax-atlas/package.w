// atlas:begin package-root
package {
  schema: "w.package/1"
  name: "atlas/syntax"
  edition: "2026"
  dependencies: [
    {
      alias: "city"
      package: "fiction/city"
      source: .registry("w")
    }
  ]
}
// atlas:end package-root
