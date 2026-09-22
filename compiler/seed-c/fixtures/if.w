fn serve(isOpen: Bool) {
  if isOpen { print("Kitchen open") } else { print("Kitchen closed") }
  print("After service")
}
fn main() { serve(isOpen: true) serve(isOpen: false) }
entry(main)
