// R1 Last Light refinement-subject study variant.

alias MenuTitle = String

enum TitleFault: Error {
  outsideBounds
}

const sampleTitle: MenuTitle = "Milliways"

fn acceptTitle(_ input: String): MenuTitle throws TitleFault {
  guard input.scalars.count in 1...40 else throw .outsideBounds
  return take input
}

test "runtime validation preserves bounded titles" for acceptTitle {
  let ordinary = try acceptTitle("Milliways")
  let boundary = try acceptTitle("1234567890123456789012345678901234567890")
  expect sampleTitle == "Milliways"
  expect ordinary == "Milliways"
  expect boundary.scalars.count == 40
}
