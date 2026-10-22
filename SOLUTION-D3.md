A big thing that came up when designing requirements for this assignment was with Slices. It was important to realize that Slices are only permitted when the values of a Sequence are classified as Copy. This is something that doesn't come up in most languages these days, though I have had to make deep copies for Java in order to not cause issues.
Another major decision was to expose recursive destruction strategies in a separate cleanup plan. The named-owner summary still counts only inserted destruction statements. The detailed plan shows owned elements being destroyed before their containing sequence storage, and identifies temporary cleanup and ownership transfers. This makes recursive cleanup inspectable without counting nested allocations as additional named owners.

`TopSequence` represents `Seq(T)` structurally in the type solver and is always
classified Own. Literal elements unify with one element type. Indexing, slicing,
and length constrain that type without consuming the sequence. A slice creates
new storage and requires Copy elements.

Borrowed iteration retains an element-to-binder constraint in a generic function
scheme: Copy elements bind directly, while Own elements bind through a borrow.
Each instantiation solves this relation. Call targets and effect summaries also
carry the restrictions needed to reject generic extraction, escaping borrows,
and cleanup for which the callee lacks the element type.

Noncapturing lambdas are represented by synthetic functions linked to their
source expressions. They reuse ordinary symbol, call-graph, type, and ownership
analysis. Source printing retains the original lambda expression.

The destruction plan consumes the validated exit states from move analysis.
Inserted named destruction nodes reference their plan actions, and recursive
recipes describe cleanup of sequence contents, references, and algebraic values.
SOP LLVM lowering and execution are deferred to Deliverable 4.
