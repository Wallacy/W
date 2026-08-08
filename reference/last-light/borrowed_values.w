// Positive M1 source oracle for place precision and lifetime-dependent values.

import * from std.memory
import std.task

export struct Oven {
  name: String
  var temperature: f64
}

export struct Kitchen {
  westOven: Oven
  eastOven: Oven
}

export struct Menu {
  title: String
  labels: Array<String>
}

// Stored borrowed fields are values with inferred origins.
export struct MenuDocument {
  title: ref String
  body: view String
}

export struct BorrowedMenu {
  // This is an owned aggregate. Its fields carry the two input origins.
  document: MenuDocument
  labels: Array<ref String>
}

// A stored inout field is a move-only exclusive capability. It does not own or
// keep the referent alive.
export struct OvenControl {
  temperature: inout f64
}

export struct OvenReadings {
  west: f64
  east: f64
}

// Known fields are disjoint places. The HIR can borrow one oven while it reads
// or mutates the other oven.
export fn readDisjointOvens(kitchen: ref Kitchen): OvenReadings {
  let west = kitchen.westOven.temperature
  let east = kitchen.eastOven.temperature
  return OvenReadings(west: west, east: east)
}

// The aggregate carries the menu and body origins. It does not keep either
// referent alive. Moving the aggregate transfers its dependency edges.
export fn borrowMenu(menu: ref Menu, body: view String): BorrowedMenu {
  let title = ref menu.title
  let document = MenuDocument(title: title, body: body)
  var labels: Array<ref String> = Array<ref String>()
  // Keep a reference to the title. The array owns only its own storage.
  labels.append(title)
  return BorrowedMenu(
    document: take document,
    labels: take labels,
  )
}

fn warmDisjointOvens(west: inout Oven, east: inout Oven) {
  west.temperature += 1.0
  east.temperature += 1.0
}

// One call creates two exclusive child loans. The fields are disjoint. The
// call ends both children and restores the Kitchen parent before the read.
export fn reborrowDisjointOvens(kitchen: inout Kitchen): OvenReadings {
  warmDisjointOvens(inout kitchen.westOven, inout kitchen.eastOven)
  return readDisjointOvens(ref kitchen)
}

fn increaseTemperature(temperature: inout f64) {
  temperature += 1.0
}

export fn reborrowWestOvenChild(kitchen: inout Kitchen): f64 {
  increaseTemperature(inout kitchen.westOven.temperature)
  // The child ended at return from increaseTemperature. The parent is usable.
  return kitchen.westOven.temperature
}

// The exclusive edge permits mutation only through the stored capability. It
// ends when control leaves scope.
export fn warmThroughStoredControl(kitchen: inout Kitchen): f64 {
  var control = OvenControl(temperature: inout kitchen.westOven.temperature)
  control.temperature += 1.0
  return control.temperature
}

// A shared child borrow is inferred from the parent borrow. The parent remains
// frozen only during this call and becomes usable after the return.
export fn menuTitle(menu: ref BorrowedMenu): ref String {
  return menu.document.title
}

// Array<ref String> owns its descriptor and storage. Each element adds a
// dependency edge. OriginSet is the deduplicated projection of those edges.
export fn collectMenuLabels(menu: ref BorrowedMenu): Array<ref String> {
  return copy menu.labels
}

fn countTitle(title: ref String): usize {
  return title.scalars.count
}

// A copied shared child keeps the same parent origin. The parent remains
// frozen until both references reach their last use.
export fn duplicatedTitleLength(menu: ref BorrowedMenu): usize {
  let title = menuTitle(menu)
  let duplicate = copy title
  return countTitle(title) + countTitle(duplicate)
}

// The child is structured. The borrow remains valid until the join and the
// task frame is stable before suspension.
export async fn stableStructuredMenuUse(menu: ref BorrowedMenu): usize {
  let title = menuTitle(menu)
  spawn<.compute> let count = countTitle(title)
  return await count
}

test "M1 borrowed menu keeps disjoint kitchen work useful" {
  var kitchen = Kitchen(
    westOven: Oven(name: "west", temperature: 180.0),
    eastOven: Oven(name: "east", temperature: 210.0),
  )
  let readings = reborrowDisjointOvens(inout kitchen)
  expect readings.west < readings.east
  expect reborrowWestOvenChild(inout kitchen) == 182.0
  expect warmThroughStoredControl(inout kitchen) == 183.0

  let menu = Menu(title: "Pan Galactic Breakfast", labels: ["safe"])
  let borrowed = borrowMenu(ref menu, body: "Served before the universe ends.")
  expect menuTitle(ref borrowed) == "Pan Galactic Breakfast"
  expect duplicatedTitleLength(ref borrowed) == 44
  let labels = collectMenuLabels(ref borrowed)
  expect labels.count == 1
}
