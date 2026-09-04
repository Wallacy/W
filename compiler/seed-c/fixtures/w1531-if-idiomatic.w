// Idiomatic candidate: name the reusable service helper.
fn serve(isOpen: Bool) {
  if isOpen {
    let message = "Kitchen open"
    print(message)
  } else {
    let message = "Kitchen closed"
    print(message)
  }
  print("After service")
}
fn main() {
  serve(isOpen: true)
  serve(isOpen: false)
}
entry(main)
