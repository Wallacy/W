module script
struct Holder {
  script: String
}
fn script(_ script: String): String {
  let script = Holder(script: script).script
  return script
}
