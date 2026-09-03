// R1 expression-core semantic-negative variant.

enum Stage {
  preparing
  serving
}

// Parseable syntax. An ordinary function body does not yield this tail.
fn nextStage(_ ready: Bool, _ trace: inout Array<u8>): Stage {
  if ready {
    trace.append(1);
    .serving
  } else {
    trace.append(2);
    .preparing
  }
}

fn recordReady(_ ready: Bool): () {
  if ready {
    let marker = 1
    marker;
  }
}

test "implicit function tail remains a semantic negative" for nextStage {
  var trace: Array<u8> = []
  // Semantic oracle rejects this function before execution.
  expect trace == []
}
