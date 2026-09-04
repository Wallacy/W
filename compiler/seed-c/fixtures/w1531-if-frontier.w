// Frontier candidate: sequential diamonds call distinct service helpers.
fn openService() {
  print("Kitchen open")
}
fn closedService() {
  print("Kitchen closed")
}
fn main() {
  if true {
    openService()
  } else {
    closedService()
  }
  print("After service")
  if false {
    openService()
  } else {
    closedService()
  }
  print("After service")
}
entry(main)
