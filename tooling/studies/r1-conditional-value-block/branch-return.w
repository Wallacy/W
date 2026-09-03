// R1 expression-core study variant.

enum Stage {
  preparing
  serving
}

fn nextStage(_ ready: Bool, _ trace: inout Array<u8>): Stage {
  if ready {
    trace.append(1);
    return .serving
  } else {
    trace.append(2);
    return .preparing
  }
}

fn recordReady(_ ready: Bool): () {
  if ready {
    let marker = 1
    marker;
  }
}

test "explicit branch returns preserve the stage" for nextStage {
  var trueTrace: Array<u8> = []
  var falseTrace: Array<u8> = []
  expect nextStage(true, inout trueTrace) == .serving
  expect nextStage(false, inout falseTrace) == .preparing
  expect trueTrace == [1]
  expect falseTrace == [2]
}
