fn serve(isOpen: Bool) {
  if isOpen { print("Kitchen open") }
  print("After service")
}
fn main() { serve(isOpen: true) }
entry(main)
