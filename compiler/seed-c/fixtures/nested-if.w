fn serve(isOpen: Bool, isKitchen: Bool) {
  if isOpen {
    print("Restaurant open")
    if isKitchen { print("Kitchen ready") }
    else { print("Kitchen closed") }
    print("Open branch joined")
  } else {
    print("Restaurant closed")
    if isKitchen { print("Kitchen ready") }
    else { print("Kitchen closed") }
    print("Closed branch joined")
  }
  print("Post-join service")
}
fn main() {
  serve(isOpen: true, isKitchen: true)
  serve(isOpen: true, isKitchen: false)
  serve(isOpen: false, isKitchen: true)
  serve(isOpen: false, isKitchen: false)
}
entry(main)
