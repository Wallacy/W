# BRX3 source-clause relation study

BRX3 fixes the pre-1.0 source form for a requirement or callable that returns
a borrowed value from more than one independent input.

The selected form is a contextual `borrows(...)` clause. The clause names
parameter slots and result ordinals. Lowering resolves names to slots and
sorts the canonical `BorrowRelation/1` payload. A body or default extension
proves the clause. A witness or provider proves the same payload. A caller
cannot choose a relation.

The host oracle does not execute W. It does not claim a compiler, HIR,
provider, linker, runtime or foreign execution. Those gates remain
implementation evidence gaps.
