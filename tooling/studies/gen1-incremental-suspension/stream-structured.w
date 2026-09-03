module gen1StreamStructured

import streaming from std.stream

export async fn observeSuspension<E: Error>(
  _ source: take some Stream<view String, E>,
): usize throws E {
  var count: usize = 0
  for try await line in source {
    count = count + line.count
  }
  return count
}

export async fn closePull<E: Error>(
  _ source: take some Stream<view String, E>,
): () throws E {
  try await (take source).cancel()
}

export async fn observeTraversal<E: Error>(
  _ source: take some Stream<view String, E>,
): usize throws E {
  var retained: usize = 0
  for try await line in source {
    let view = ref line
    retained = retained + view.count
  }
  return retained
}

export async fn observeDelegation<E: Error>(
  _ source: take some Stream<view String, E>,
): usize throws E {
  let child = async observeTraversal(take source)
  return try await child
}
