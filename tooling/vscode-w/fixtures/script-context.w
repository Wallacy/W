// TextMate negative fixture: only the root header is a script keyword.
script {
  edition: "2026"
}

module script
struct Holder {
  script: String
}
fn script(script: String): String {
  let script = Holder(script: script).script
  return script
}
