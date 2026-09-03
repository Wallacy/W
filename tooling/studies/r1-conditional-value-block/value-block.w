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

test "only the selected branch produces the stage" for nextStage {
  var trueTrace: Array<u8> = []
  var falseTrace: Array<u8> = []
  expect nextStage(true, inout trueTrace) == .serving
  expect nextStage(false, inout falseTrace) == .preparing
  expect trueTrace == [1]
  expect falseTrace == [2]
}
