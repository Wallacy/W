/// Generic protocols, primary associated types, and deterministic witnesses.
import si from std
import { PhysicalDuration } from units

export protocol Source<Item> {
  fn first(): ref Item?
}

export protocol Counted {
  fn count(): usize
}

export protocol Catalog<Item>: Source<Item> & Counted {}

export struct Shelf<T> {
  values: Array<T>
}

export struct StaticValue<T, _ value: T> {
  export const expected = value
}

export const fn isFinalCallLabel(value: String): Bool {
  return value == "The final seating"
}

export struct FinalCallValue<_ value: String<(isFinalCallLabel(.member))>> {
  export const expected = value
}

export struct StaticWindow<
  _ start: usize,
  _ end: usize,
  unit: PhysicalDuration,
> {
  export const span = end - start
}

export struct Matrix<Element, rows: usize, columns: usize> {
  export const area = rows * columns
}

export alias EnabledFeature = StaticValue<Bool, true>
export alias LastCallLabel = StaticValue<String, "The final seating">
export alias VerifiedFinalCall = FinalCallValue<"The final seating">
export alias LastCallDeadline = StaticValue<PhysicalDuration, 10<si.s>>
export alias HorizonWindow = StaticWindow<0, 60, unit: 1<si.s>>

extension<T: Display & Equatable> Shelf<T>: Catalog {
  alias Item = T

  fn first(): ref T? {
    return values.first
  }

  fn count(): usize {
    return values.count
  }
}

export fn firstEquals<T: Equatable, S: Source<T>>(
  source: ref S,
  named expected: ref T,
): Bool {
  guard let ref item = source.first() else return false
  return item == expected
}

export fn renderFirst<T: Display, S: Source<T>>(source: ref S): String? {
  guard let ref item = source.first() else return .none
  return item.display()
}

test "generic inference uses the declared witness" for firstEquals {
  let shelf = Shelf(values: ["Pan-Galactic Broth", "Horizon Cake"])

  expect firstEquals(shelf, expected: "Pan-Galactic Broth")
  expect renderFirst(shelf) == "Pan-Galactic Broth"
  expect shelf.count() == 2
}

test "contract atoms preserve their compile-time kind" {
  expect EnabledFeature.expected
  expect LastCallLabel.expected == "The final seating"
  expect VerifiedFinalCall.expected == "The final seating"
  expect LastCallDeadline.expected == 10<si.s>
  expect HorizonWindow.span == 60
  expect Matrix<f32, rows: 3, columns: 4>.rows == 3
  expect Matrix<f32, rows: 3, columns: 4>.area == 12
}
