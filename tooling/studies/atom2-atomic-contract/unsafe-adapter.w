// ATOM2 permitted shape. A concrete adapter still needs implementation evidence.

module atom2_unsafe_adapter

export unsafe fn reclaimAfterDrain(_ domain: String): () {
  // register/access/exit, unlink/retire, quiescence/drop/reclaim,
  // participant drain, callback drain, and shutdown are receipt obligations.
}
