// R1 Last Light refinement-subject study variant.

type MenuTitle = String<(value.scalars.count in 1...40)>

enum TitleFault: Error {
  outsideBounds
}

const sampleTitle: MenuTitle = "Milliways"

fn acceptTitle(input: String): MenuTitle throws TitleFault {
  do {
    return try MenuTitle(take input)
  } catch {
    throw .outsideBounds
  }
}

test "explicit refinement subject preserves bounded titles" for acceptTitle {
  let ordinary = try acceptTitle("Milliways")
  let boundary = try acceptTitle("1234567890123456789012345678901234567890")
  expect sampleTitle == "Milliways"
  expect ordinary == "Milliways"
  expect boundary.scalars.count == 40
}
