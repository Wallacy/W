// Learner candidate: duplicate the two service choices inline.
fn main() {
  if true {
    print("Kitchen open")
  } else {
    print("Kitchen closed")
  }
  print("After service")
  if false {
    print("Kitchen open")
  } else {
    print("Kitchen closed")
  }
  print("After service")
}
entry(main)
