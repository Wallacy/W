// R1 expression-core study variant.

enum Stage {
  preparing
  serving
}

fn nextStage(_ ready: Bool, _ trace: inout Array<u8>): Stage {
  var e: u8 = 0

  return if ready {
    e += 1;
    trace.append(e);
    .serving
  } else {
    e += 2;
    trace.append(e);
    .preparing
  }
}

fn recordReady(_ ready: Bool): () {
  if ready {
    let marker = 1
    marker;
  }
}

test "mutable temporary records only the selected branch" for nextStage {
  var trace: Array<u8> = []
  expect nextStage(true, inout trace) == .serving
  expect trace == [1]
}
