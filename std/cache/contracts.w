// Bounded process-local cache contracts.

import { Duration } from std.time

export enum Expiration {
  none
  afterAccess(Duration)
  afterWrite(Duration)
}

export struct LocalBinding<
  Key: Equatable & Hashable & Duplicable,
  Value: Duplicable,
> {
  let name: String
  let maximumEntries: usize
  let maximumActiveLoads: usize
  let maximumQueuedLoads: usize
  let expiration: Expiration

  export const init(
    name: String,
    maximumEntries: usize<(1...)>,
    maximumActiveLoads: usize<(1...)>,
    maximumQueuedLoads: usize,
    expiration: Expiration = .none,
  ) {
    self.name = name
    self.maximumEntries = maximumEntries
    self.maximumActiveLoads = maximumActiveLoads
    self.maximumQueuedLoads = maximumQueuedLoads
    self.expiration = expiration
  }
}

export enum CacheError: Error {
  unavailable
  overloaded
  resourceExhausted
}

export enum LoadFailure<Failure: Error>: Error {
  cache(CacheError)
  loader(Failure)
}

export protocol LocalCache<
  Key: Equatable & Hashable & Duplicable,
  Value: Duplicable,
> {
  fn get(key: ref Key): Value?
  fn insert(key: Key, value: Value): () throws CacheError
  fn invalidate(key: ref Key)
  fn invalidateAll()

  async fn getOrLoad<Failure: Error>(
    key: Key,
    using loader: some async fn(ref Key): Value throws Failure,
  ): Value throws LoadFailure<Failure>
}
