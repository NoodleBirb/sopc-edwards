# Deliverable 3 Requirements

Each construct section describes typing, ownership, and memory-safety behavior.
The invariant map identifies the machinery responsible for enforcing that
behavior.

## Invariant Map

Rows group related invariants; requirement IDs identify their detailed rules.
Existing rules are reused where noted, with new construct support added to the
relevant passes.

| Requirements | Invariant | Enforcing machinery and reason |
|---|---|---|
| LA2; existing construct scopes | Names resolve in their permitted scope; lambdas capture no surrounding locals. | SymbolTable and name collectors: reuse Deliverable 2 scope behavior. |
| SL1, IX1, IX4, LN1–LN2, SC1, RG1, BI1–BI2, LA1, LA4 | Operands, results, binders, and call arities have compatible types. | TypeInference: extend constraints; reuse existing unification and ordinary call checks. BI2 also needs element ownership classification. |
| SL2 | An undetermined literal element type is rejected. | Post-inference validation: inspect solved results; unification success alone is insufficient. |
| SL3, SC2, RG2, BI2, BI5, LA3 | Containers are Own; integer, borrow, and function values are Copy. | OwnershipClassifier: inspect inferred type structure; extend sequence/binder coverage. |
| SL4–SL5, SL11–SL13, LN3, SC2, RG3, BI6, LA6 | Transfers update state; moved uses, live-owner overwrites, and incompatible joins/iterations are rejected. | MoveAnalysis: carry state through the AST; reuse state rules and extend traversal for new forms. |
| SL5, SL10, IX2–IX4, IX6, LN4, BI1, BI4, LA6 | Borrow positions, element extraction, escape, and mutation obey the access restrictions. | Borrow-checking machinery, using inferred types/classes: inspect expression context and extend loop/index handling. |
| IX5, BI3 | Element and loop borrows protect the originating sequence owner. | BorrowChecker identifies provenance; summaries propagate it across calls; MoveAnalysis rejects conflicting moves. |
| SL9, SC3–SC4, LA5 | Calls satisfy instantiated cleanup/Copy requirements and account for possible lambda targets. | CallGraph identifies bodies; FunctionEffectSummaries records obligations; call-site validation checks instantiated types. |
| SL10, LA6 | Returning ownership transfers it into the call result. | FunctionEffectSummaries describes return origin; MoveAnalysis applies the transfer. |
| SL6–SL8, SL18, LN5, SC5, RG2, BI5, LA3, LA6 | An inspectable semantic plan accounts for retained owners, recursive cleanup, and untransferred temporaries. | MoveAnalysis and DestructionPass use ownership/type information to establish cleanup targets, points, and type-directed recipes; destruction insertion and future lowering share that information. Runtime frees belong to Deliverable 4. |
| RG4, BI6 | Loops expose body, continuation, and repetition paths. | CFGBuilder constructs blocks/edges; CFGRenderer displays them. |
| SL14–SL17, SL19, IX7–IX9, LN6–LN7, SC6–SC7, RG5–RG6, BI7–BI9, LA7–LA8 | Views expose types, classes, named-destroy counts, recursive cleanup plans, moves, and borrow provenance. | Existing inspection renderers consume the corresponding analysis results; detailed cleanup inspection reads SL18's semantic plan without redefining the named-owner count. |

## Sequence Literals

Literal construction introduces no new source-level control-flow construct.
Existing name-resolution and expression-validity rules continue to apply.

### Typing

**SL1 — Element expressions and typing.** A sequence literal shall accept element
expressions, including literals, variable references, and function calls,
subject to existing expression-validity rules. All element types shall unify
with one common element type `T`. The resulting sequence type shall be
`Seq(T)`. This relationship shall also apply to nested sequences.

**SL2 — Undetermined element type.** After type inference, a sequence literal
shall be rejected if its element type remains undetermined by the available
typing context. In particular, an empty literal for which nothing determines
the element type shall be rejected. Polymorphic function schemes remain
permitted.

### Ownership and Construction

**SL3 — Ownership classification.** A sequence value shall be classified Own,
regardless of whether its elements are classified Copy or Own.

**SL4 — Owned elements.** Placing an owned variable's value into a sequence
literal shall transfer ownership of that value into the sequence and mark the
source binding Moved.

**SL5 — Copy elements.** Using a Copy-class variable as an element shall copy
its value without moving the source binding. Existing restrictions on storing
borrowed values shall continue to apply.

### Destruction

**SL6 — Responsibility for contents.** A sequence shall own its Own-class
elements. Destruction of the sequence shall recursively destroy those elements
and release the sequence's own storage. Copy-class elements shall require no
destruction. The semantic plan specified in SL18 shall represent destruction of
owned contents before release of the containing sequence's storage.
Implementing the actual runtime frees belongs to Deliverable 4.

**SL7 — Named-owner destruction.** A named sequence that remains Owned at
function exit shall receive a destruction operation before the function
returns, after computing the return value. A binding whose ownership has
transferred elsewhere, including through the return value, shall not destroy
that transferred value. The cleanup target and its ordering relative to return
evaluation shall be represented in SL18's plan.

**SL8 — Unnamed sequence cleanup.** An unnamed sequence used by an operation
that does not retain or transfer its ownership shall be destroyed once that
operation no longer needs it. If ownership transfers into a binding, another
function, or a returned result, cleanup responsibility shall follow that
transfer. SL18's plan shall identify required temporary cleanup and its point
of execution; it shall not schedule an additional cleanup for a temporary whose
ownership has transferred into another owner, including a sequence element.

### Function Boundaries and Borrowing

**SL9 — Cleanup safety across calls.** A by-value call transferring a sequence
containing Own-class elements shall be rejected when the callee must destroy
the sequence but lacks the element-type information needed to destroy its
contents. This restriction shall not prohibit a callee with sufficient cleanup
information from accepting and destroying the sequence.

**SL10 — Borrowing and returning sequences.** Borrowing a sequence shall leave
ownership with the original owner. Returning an owned sequence shall transfer
ownership into the call's result. In either case, the callee shall not destroy
a sequence whose ownership it does not retain. Existing type, move, and borrow
restrictions shall continue to apply.

### Inherited Move Rules

**SL11 — Use after move.** A sequence binding in the Moved state shall not be
used until it receives a new value. Attempts to read, borrow, or transfer its
moved value shall be rejected.

**SL12 — Overwriting a live owner.** Assignment shall be rejected if it would
overwrite a sequence binding that still owns its previous value. Transferring
the previous value's ownership out of the binding shall remove that ownership
obligation.

**SL13 — Ownership at branch joins.** Alternative control-flow paths shall
leave compatible ownership states for sequence bindings at their join. A
sequence binding that is Owned on one path and Moved on another shall cause
rejection.

### Inspection Views

**SL14 — Destruction-summary reporting.** The destruction summary shall count
inserted destruction statements for named owners. It shall not count
unnamed-temporary cleanup or the individual resources released recursively by
a destruction operation.

**SL15 — Type inspection.** `--ptype` shall display sequence types as `Seq(T)`,
where `T` is the inferred element type, including nested sequence types.

**SL16 — Ownership inspection.** `--pownership` shall report named sequence
bindings as Own and display the destruction summary specified in SL14. Each
named sequence binding that remains Owned at function exit shall contribute
one inserted destruction operation to that summary.

**SL17 — Ownership constraints and traces.** With
`--pownership --constraint`, sequence ownership establishment and transfers
shall appear using the existing ownership-trace conventions.

**SL18 — Inspectable semantic destruction plan.** Destruction analysis shall
retain an inspectable plan for sequence cleanup. Each cleanup shall identify
its named owner or unnamed source expression, inferred type, cleanup point,
and type-directed destruction strategy. Strategies shall represent recursive
cleanup of Own-class contents, omit destruction of Copy-class contents, and
release sequence storage after its owned contents. Transferred ownership shall
be accounted for at its recipient rather than cleaned up twice.

The plan may describe per-element cleanup and reference recursive strategies;
it need not statically enumerate runtime elements or predict a runtime free
count. Destruction insertion and eventual Deliverable 4 lowering shall use the
same semantic cleanup information made available for inspection.

**SL19 — Destruction-plan inspection.** With `--pownership --constraint`,
additional detailed output shall expose SL18's cleanup targets, types, cleanup
points, and recursive destruction strategies. Named owners shall be identified
by their bindings; unnamed values shall be identifiable by their source
expressions and locations. The report shall read the semantic plan rather
than independently reconstruct an explanatory model. This detail shall remain
separate from the named-owner destruction summary and shall not change the
count specified in SL14.

## Indexing

**IX1 — Types.** An indexing expression shall require a base of type `Seq(T)`
and an index of type `int`. Its result type shall be `T`. Operand expressions
shall be accepted according to their inferred types, rather than restricted
to particular syntactic forms.

**IX2 — Copy-class elements.** Reading a Copy-class element by value shall copy
that element without transferring ownership of the sequence.

**IX3 — Own-class elements.** Taking an Own-class element by value through
indexing shall be rejected. Indexing shall not remove the element or transfer
its ownership out of the sequence. This restriction shall also apply when
accessing a sequence through a borrowed reference.

**IX4 — Borrowing an element.** Borrowing an indexed element of type `T` shall
produce borrowed access of type `borrow&T`, without transferring ownership of
the element or sequence. Existing borrow-position and escape restrictions
shall apply.

**IX5 — Borrow provenance and lifetime.** An indexed-element borrow shall be
associated with the owner of the underlying sequence storage. That owner shall
remain available throughout the borrow's permitted lifetime. Moving the
sequence during a call that borrows one of its elements shall be rejected,
regardless of argument order.

**IX6 — Mutation through borrowed access.** Borrowed access obtained through
indexing shall follow existing mutation restrictions: modifying Copy-class
data shall be permitted, while replacing an owned value through borrowed
access shall be rejected. Copy classification of the borrowed reference shall
not permit storing it for later use.

Existing use-after-move and temporary-cleanup requirements also apply to
indexing expressions.

**IX7 — Type inspection.** `--ptype` shall report the inferred type `T` for
bindings receiving a Copy-class indexed element from `Seq(T)`.

**IX8 — Borrow inspection.** For accepted indexed-element borrows, `--pborrow`
shall report the borrowed expression, its source location, and its approval.

**IX9 — Borrow explanation.** With `--pborrow --constraint`, detailed output
shall explain the borrow's permitted use context and provenance. For a direct
call argument, this shall identify the receiving call and argument position.

## Length

**LN1 — Types.** Length shall require an operand of type `Seq(T)` and produce a
result of type `int`.

**LN2 — Element-type independence.** Length shall impose no additional
constraint on the sequence's element type or ownership class. It shall not
determine an otherwise unresolved element type; SL2 shall continue to apply.

**LN3 — Ownership.** Inspecting an existing sequence's length shall not
transfer its ownership. Existing use-after-move restrictions shall continue
to apply.

**LN4 — Borrowed access.** Length shall be permitted on a sequence accessed by
dereferencing a borrowed reference, subject to existing borrow restrictions.
This access shall not transfer ownership of the sequence.

**LN5 — Temporary cleanup.** When length consumes an unnamed sequence result
without transferring its ownership onward, temporary cleanup shall follow SL8
after the length has been obtained. That cleanup point shall be represented in
SL18's plan and exposed under SL19.

**LN6 — Type inspection.** `--ptype` shall report `int` for a binding receiving
the result of a length expression.

**LN7 — Ownership inspection.** With `--pownership --constraint`, inspecting
an existing sequence's length shall not itself produce a move event for that
sequence. Other ownership effects from evaluating the operand shall continue
to follow the existing rules.

## Slicing

**SC1 — Types.** Slicing shall require a base of type `Seq(T)` and bounds of
type `int`. The resulting type shall be `Seq(T)`.

**SC2 — Separate storage and ownership.** A slice shall allocate separate
sequence storage and produce a new Own-class value. Creating the slice shall
not transfer ownership of the original sequence.

**SC3 — Copying elements.** A slice shall copy the selected elements. Slicing
shall be permitted only when the element type is classified Copy. When that
class depends on a call-site instantiation, the Copy restriction shall be
enforced for that instantiation.

**SC4 — Owned-element rejection.** Slicing a sequence whose element type is
classified Own shall be rejected because copying would duplicate ownership.
This includes nested sequences whose elements are themselves sequences.
Slicing shall not perform deep cloning.

**SC5 — Cleanup.** Ownership and destruction of the resulting slice shall
follow SL6–SL8 and SL11–SL13. Cleanup obligations arising from evaluating the
source expression shall continue to apply independently. Required cleanup of
the slice and any source temporary shall be represented in SL18's plan.

**SC6 — Type inspection.** `--ptype` shall report `Seq(T)` for a binding
receiving a slice from a base of type `Seq(T)`.

**SC7 — Ownership inspection.** A named slice result shall be reported as Own
under SL16. Its destruction-summary and ownership-trace reporting shall follow
SL14 and SL17. Creating the slice shall not itself produce a move event for
the source sequence; other effects from evaluating its operands shall continue
to follow the existing rules. Detailed cleanup inspection shall follow SL19.

## Range Loops

Name resolution and binder scope continue to follow the established frontend
behavior.

**RG1 — Types.** Range bounds and the loop binder shall have type `int`. Bounds
shall be checked according to their inferred types.

**RG2 — Ownership.** The range and its integer binder shall introduce no owned
heap resource or destruction obligation. Ownership effects within the body
shall follow the existing rules.

**RG3 — Ownership across iterations.** The body shall preserve compatible
ownership states for externally scoped bindings across iterations and at loop
exit. Incompatible states shall be rejected under the existing loop ownership
rules.

**RG4 — Control-flow graph.** A range loop shall have the same general CFG
structure as a `while` loop: a path into the body, a path to the continuation
without executing the body, and a path from the body back to the loop decision.

**RG5 — Type inspection.** `--ptype` shall report the range-loop binder as
`int`.

**RG6 — Ownership inspection.** `--pownership` shall report the integer binder
as Copy. No destruction operation shall be inserted solely for that binder.

## Borrowed Sequence Iteration

**BI1 — Iterable type.** Sequence iteration shall require an iterable of type
`borrow&Seq(T)`. A direct borrow of a sequence owner shall be permitted in the
loop header. An expression already having the required borrowed type shall be
accepted without another borrow. A plain `Seq(T)` iterable shall be rejected.

**BI2 — Binder type.** For Copy-class elements, the binder shall have type `T`.
For Own-class elements, it shall have type `borrow&T`, providing access without
transferring element ownership.

**BI3 — Borrow lifetime.** The underlying sequence owner shall remain
available throughout the entire loop. Moving that owner while the loop borrows
it shall be rejected, regardless of the element class.

**BI4 — Borrowed-binder restrictions.** An Own-element borrowed binder shall
only be passed onward as an immediate function argument. It shall not be
stored or returned, and access through it shall not transfer ownership of the
element. Permitted mutation through borrowed access shall follow IX6.
Copy-element binders shall follow ordinary Copy-value rules.

**BI5 — Ownership and destruction.** Iteration shall not transfer ownership of
the sequence or its elements. Neither a Copy-element binder nor a borrowed
Own-element binder shall receive a destruction operation. The sequence's owner
shall retain its cleanup responsibility.

**BI6 — Control flow.** Borrowed iteration shall follow the loop
ownership-state and CFG requirements in RG3–RG4.

**BI7 — Type inspection.** `--ptype` shall report the binder as `T` for
Copy-class elements and `borrow&T` for Own-class elements.

**BI8 — Ownership inspection.** `--pownership` shall report both binder forms
as Copy. Iteration shall leave the sequence owner's ownership unchanged and
shall introduce no destruction operation for the binder.

**BI9 — Borrow inspection.** `--pborrow` shall report an approved loop-header
borrow with its expression and source location. With `--constraint`, the
report shall identify the loop-iterable context and relevant provenance,
distinguishing it from approval as a direct call argument.

## Lambdas and Calls

**LA1 — Type inference.** Lambda parameter and result types shall be inferred
using constraints from the body and applicable call sites. The lambda shall
have an ordinary function type relating its parameter types to its result
type.

**LA2 — Scope.** Lambda parameters shall be resolved within the lambda's
parameter scope. Surrounding local bindings shall not be captured. Other
name-resolution behavior shall follow the established frontend rules.

**LA3 — Ownership classification.** A noncapturing lambda value shall be
classified Copy and shall not require destruction solely because it is a
function value. This classification shall not prevent its arguments from
carrying ownership obligations.

**LA4 — Call checking.** Lambda calls shall use the existing function-call
typing rules, including argument-type and argument-count checks. Lambda values
shall be usable as arguments to compatible higher-order functions.

**LA5 — Call targets and effects.** Calls through lambda values shall resolve
to their possible callable bodies. Those bodies shall participate in
function-effect summaries so their argument consumption, return ownership, and
cleanup requirements can be checked.

**LA6 — Ownership at calls.** Calls involving lambda values shall apply the
existing move, borrow, and destruction rules to their arguments and results.
Copy classification of the function value shall not suppress ownership
transfers involving its arguments.

**LA7 — Type inspection.** `--ptype` shall expose each lambda's inferred
function type and parameter types. Lambda entries shall be identifiable by
their source expression and location, following the handout's source-based
identification convention.

**LA8 — Ownership inspection.** Ownership inspection shall expose the relevant
ownership classes and destruction information for lambda parameters and local
bindings. With `--pownership --constraint`, argument and result ownership
transfers shall follow the existing trace conventions.

# Deliverable 3 Tests

The groups below collect related requirements for designing tests. A group can
use several tests, and a test can cover requirements from several groups.

## G1 — Literal types and inference

**Requirements:** SL1, SL2, SL15.

- **SL1:** Element expressions must unify with one common type `T`; the literal
  has type `Seq(T)`, including nested sequences.
- **SL2:** Reject a literal whose element type remains undetermined after
  inference.
- **SL15:** The type view displays `Seq(T)`, including nested element types.

**Assertions / test design:**

- Create separate homogeneous sequences using each element type below. Require
  acceptance and inspect `--ptype` for the corresponding sequence type:

  | Element type `T` | Expected sequence type |
  |---|---|
  | `int` | `Seq(int)` |
  | `own&int` | `Seq(own&int)` |
  | `Seq(int)` | `Seq(Seq(int))` |
  | `Seq(own&int)` | `Seq(Seq(own&int))` |

- Require rejection of a literal mixing `own&int` and `int` elements.
- Require rejection of a literal mixing `int` and `Seq(int)` elements.
- Require rejection of an empty sequence literal when no surrounding typing
  information determines its element type.
- Negative assertions shall identify incompatible-element types or an
  undetermined element type, respectively.
- Include expression elements, such as `[seed, seed + 1, inc(seed)]` with integer `seed` and
  `inc(x) { return x + 1; }`; require `Seq(int)`.

## G2 — Construction, moves, overwrites, and joins

**Requirements:** SL4, SL5, SL11, SL12, SL13, SL17.

- **SL4–SL5:** Owned element sources move into a literal; Copy sources remain
  available. Existing borrow-storage restrictions still apply.
- **SL11–SL12:** Moved bindings cannot be used until reinitialized; a live
  sequence owner cannot be overwritten.
- **SL13:** Ownership states must agree where alternative paths join.
- **SL17:** Detailed ownership output records establishment and transfers.

**Assertions / test design:**

- Check that placing an owned source binding's value into a sequence literal
  changes that source binding from Owned to Moved and transfers ownership into
  the sequence. Its type's Own classification does not change.
- Check that placing a Copy-class source value into a literal leaves its source
  binding available for subsequent use.
- Require rejection when a sequence binding still Owned is assigned a new
  sequence literal.
- Require acceptance when a sequence binding whose previous value has moved
  receives a new sequence literal, restoring its ownership state.
- Inspect ownership establishment and transfer events in the detailed trace
  for the applicable cases above.
- Require rejection when a binding in the Moved state is used.
- Require rejection when alternative branches leave a sequence binding in
  incompatible ownership states.
- Also cover RG3 with ordinary and zero-iteration loop cases: require rejection
  when the body moves an external sequence owner and leaves its state changed.
  The inherited analysis checks the body conservatively even when the loop
  would execute zero times.
- SL5's borrow-storage rejection is covered by G5.

## G3 — Sequence ownership and cleanup

**Requirements:** SL3, SL6, SL7, SL8, SL14, SL16, SL18, SL19.

- **SL3:** Every sequence is Own, regardless of its element class.
- **SL6:** Cleanup accounts recursively for owned contents and sequence
  storage; Copy elements need no destruction.
- **SL7–SL8:** Remaining named owners need destruction; unnamed values need
  cleanup when their operation no longer needs them, unless ownership transfers.
- **SL14, SL16:** Ownership output reports named sequence bindings as Own;
  the destruction summary counts named-owner destroy statements, not runtime
  frees or unnamed-temporary cleanup.
- **SL18:** An inspectable semantic plan identifies cleanup owners/temporaries,
  types, cleanup points, and recursive per-element strategies. Destruction
  insertion and future lowering share this information.
- **SL19:** Detailed ownership output exposes that plan separately from the
  named-owner count.

**Assertions / test design:**

- Create sequences containing Copy-class elements and sequences containing
  Own-class elements; check that each sequence itself is classified Own.
- Check that named sequence bindings still Owned at function exit receive
  destruction. Exclude moved-from bindings whether ownership transferred to
  another owner inside the function or left through a call/return.
- Inspect the recursive plan: Copy elements have no element destruction;
  Own elements and nested sequences have the corresponding cleanup strategies.
  Owned contents are destroyed before their containing sequence storage.
- Check that an untransferred temporary receives cleanup once it is no longer
  needed. A transferred temporary must not receive a duplicate cleanup at the
  transfer source.
- Check that recursive detail associates nested cleanup with its enclosing
  owner, and identifies unnamed temporaries separately. The named-destruction
  count includes only named owners still Owned at exit; nested cleanup actions
  and temporary cleanup do not increase that count.

## G4 — Index types and taking elements by value

**Requirements:** IX1, IX2, IX3, IX7.

- **IX1:** The base is `Seq(T)`, the index is `int`, and the result type is `T`;
  operands are governed by inferred type rather than a fixed syntax shape.
- **IX2:** Reading a Copy element copies it without moving the sequence.
- **IX3:** Taking an Own element by value is forbidden, including through a
  borrowed sequence; indexing does not remove an element.
- **IX7:** The type view reports the inferred Copy-element result type.

**Assertions / test design:**

- Index an integer sequence and check that the result has type `int`. The
  element is copied, not moved, and reading it does not transfer sequence
  ownership.
- Require rejection of taking an Own-class element by value, including when
  the sequence is accessed through a borrowed reference.
- Inspect `--ptype` for the inferred indexed result type according to IX7.
- Reject indexing a non-sequence (`n[0]`
  with integer `n`) and using a sequence as an index (`s[s]`). In separate
  accepted programs, index a sequence returned by a function and use an integer
  expression or function call as the index; check the inferred result type and
  temporary cleanup plan.

## G5 — Indexed borrows, mutation, and lifetime

**Requirements:** IX4, IX5, IX6, IX8, IX9.

- **IX4:** Borrowing an element produces `borrow&T` without moving ownership;
  existing borrow-position and escape restrictions remain.
- **IX5:** The borrow protects the underlying sequence owner throughout its
  permitted lifetime, including across all arguments of a call.
- **IX6:** Borrowed access may modify Copy data but cannot replace an owned
  value or permit storing the borrow for later.
- **IX8–IX9:** Borrow output identifies the expression, source location,
  approval, and, with detail enabled, use context and provenance.

**Assertions / test design:**

- Check that creating indexed borrowed access does not move ownership of the
  sequence or element.
- Check that the underlying sequence owner remains protected for the complete
  permitted borrow lifetime, including the whole receiving call.
- Require acceptance of permitted modifications to Copy-class data reached
  through borrowed access, and rejection of replacing an owned value through
  that access.
- Inspect borrow reports for the required expression, source location,
  approval, context, and provenance.
- In independent programs with `s = [alloc 1]`,
  reject `p = &s[0]` and `u = [&s[0]]` as forbidden borrow storage.
  Also reject moving and borrowing the same source in one
  call in either argument order: use `take(q, p) { return #q + *p; }` with
  `take(s, &s[0])` for integer sequence `s`, then an independent variant
  `take(p, q) { return *p + #q; }` with `take(&s[0], s)`.

## G6 — Length, borrowed reads, and temporary operands

**Requirements:** LN1, LN2, LN3, LN4, LN5, LN6, LN7.

- **LN1–LN2:** Length takes `Seq(T)` and returns `int`; it neither constrains
  the element type/class nor resolves an otherwise undetermined literal type.
- **LN3–LN4:** Reading length preserves ownership and permits borrowed access;
  existing availability and borrow restrictions apply.
- **LN5:** Untransferred temporary operands follow SL8 cleanup.
- **LN6–LN7:** Views report an integer result and no move caused by the length
  read itself; evaluating the operand can have separate effects.

**Assertions / test design:**

- Check that length produces an integer for sequences with multiple different
  element types.
- Check that the length read itself transfers no sequence ownership and permits
  access through a borrowed sequence reference, as in `#*p`.
- Inspect the cleanup plan to check that an unnamed, untransferred sequence
  operand is cleaned up after its length has been obtained. Reading a named
  sequence's length must not prematurely destroy that existing owner.
- Check the required integer-result type display and the absence of a move
  caused by the length read in ownership output.
- Reject `#n` when `n` is an integer, and
  reject `#[]` because length supplies no element-type information. Assert
  distinct type-mismatch and undetermined-element diagnostics, respectively.

## G7 — Copying slices and their owners

**Requirements:** SC1, SC2, SC3, SC4, SC5, SC6, SC7.

- **SC1:** A `Seq(T)` base and integer bounds produce `Seq(T)`.
- **SC2:** A slice has separate owned storage; creating it does not move the
  source sequence.
- **SC3–SC4:** Elements must be Copy at the applicable instantiation. Own
  elements, including nested sequences, cannot be sliced; slicing is not cloning.
- **SC5:** Slice results follow SL6–SL8 and SL11–SL13; source evaluation has
  its own cleanup obligations.
- **SC6–SC7:** Views expose the result type, Own classification, named
  destruction information, and no source move caused by slicing itself.

**Assertions / test design:**

- Check that slicing produces a new `Seq(T)` with its own Own-class value
  and separate storage, without moving the source sequence.
- Check that slicing rejects element types that are not Copy.
- Check that slice results and temporary operands follow the normal sequence
  cleanup requirements.
- Check the required result-type, ownership, destruction, and transfer views.
- Independently reject an integer slice base,
  a sequence-valued lower bound, and a sequence-valued upper bound.
  Pair generic `cut(p) { return (*p)[0:1]; }` calls with borrowed integer
  and owned-element sequences: accept the former, reject the latter for its
  element-Copy obligation.

## G8 — Range-loop typing, ownership, and control flow

**Requirements:** RG1, RG2, RG3, RG4, RG5, RG6.

- **RG1–RG2:** Bounds and binder are integers; the range and binder introduce
  no owned resource. Body effects retain their ordinary ownership rules.
- **RG3:** External bindings must retain compatible states across iterations
  and loop exit.
- **RG4:** The CFG exposes body, continuation, and repetition paths.
- **RG5–RG6:** Views report an `int`, Copy-class binder with no binder destroy.

**Assertions / test design:**

- Check that non-integer range bounds are rejected and the loop binder is
  inferred as `int`.
- Check that the range and binder introduce no owned resources, while body
  operations follow the normal ownership rules.
- Check that external ownership states must be compatible across iterations
  and loop exit, including the move cases already described in G2.
- Check that the CFG has the expected body, exit, and repetition paths.
- Check that views show the binder as `int` and Copy, with no binder destruction.

## G9 — Borrowed iteration: input and binder types

**Requirements:** BI1, BI2, BI5, BI7, BI8.

- **BI1:** The iterable must be `borrow&Seq(T)`. Direct header borrows and
  already-borrowed expressions are permitted; a plain sequence is rejected.
- **BI2:** Copy elements give a `T` binder; Own elements give `borrow&T`.
- **BI5:** Iteration transfers no sequence/element ownership and introduces
  no binder destruction; the sequence owner retains cleanup responsibility.
- **BI7–BI8:** Views expose those binder types, classify both forms as Copy,
  and preserve the sequence owner's ownership.

**Assertions / test design:**

- Accept an iterable of type `borrow&Seq(T)`, supplied either by a header
  borrow or an already-borrowed parameter.
- Check that Copy elements give a binder of type `T`, Own elements give a
  binder of type `borrow&T`, and both binders are classified Copy.
- Check that iteration causes no ownership transfer and no binder destruction.
- Check the required type and ownership views.
- For `s = [1, 2]`, accept
  `for (e : &s) { output e; }`, then read `#s`. For
  `s = [alloc 1, alloc 2]`, accept a loop that passes `e` immediately to
  `readElem(p) { return **p; }`. In either program main has one named destroy
  targeting `s`, with none for `e`. Independently reject `for (e : s)`.
- Accept `total(q)` with `for (e : q)` and integer accumulation, called with
  `total(&s)` for an integer sequence. Check `q : borrow&Seq(int)`, no destroys
  in `total`, one for `s` in main, and that a later `#s` remains valid. Require
  full semantic acceptance, not only successful type output.

## G10 — Borrowed iteration: protected lifetime and binder uses

**Requirements:** BI3, BI4, BI6, BI9.

- **BI3:** The owner remains protected from moves for the entire loop,
  regardless of element class.
- **BI4:** An Own-element borrowed binder may only be passed as an immediate
  call argument; it cannot be stored, returned, or used to take element
  ownership. Mutation follows IX6; Copy-element binders follow Copy rules.
- **BI6:** Loop state and CFG rules are inherited from RG3–RG4.
- **BI9:** Borrow inspection identifies the header expression and location;
  detailed output explains its loop-iterable context and provenance.

**Assertions / test design:**

- Accept an Own-element loop over `s = [alloc 1, alloc 2]`: pass `e` immediately to
  `readElem(p) { return **p; }`. Check that this reads an integer without taking
  an element's ownership. For Copy elements, accept ordinary integer use such
  as accumulating `acc = acc + e`.
- Reject moving `s` while iterating `&s`, for both Copy and Own element types
  using `t = s` in the body. Assert the borrow/move conflict.
- Reject storing the Own-element borrowed binder with `keep = e` or `[e]`;
  reject taking the owned value with `keep = *e`. Test these independently so
  one failure does not mask another.
- Reject an attempt to return a borrowed binder through a forwarding callee
  whose result escapes. Assert the borrow-escape cause.
- Accept modifying Copy data reached through a properly forwarded element
  borrow; reject replacing the owned element itself. For an Own-element binder,
  pass `e` to `bump(p) { **p = **p + 1; return 0; }` as the accepted case;
  independently pass it to `replace(p) { *p = alloc 99; return 0; }` and reject
  overwriting the owned reference.
- Check the loop CFG's body, continuation, and back edge, and compatible
  external ownership states using the same assertions as G8.
- Inspect `--pborrow --constraint`: identify `&s`, its source location,
  approval, loop-iterable context, and originating owner.

## G11 — Lambda types, scope, and function values

**Requirements:** LA1, LA2, LA3, LA4, LA7.

- **LA1:** Body and call constraints infer an ordinary function type.
- **LA2:** Parameters use their lambda scope; surrounding locals are not
  captured; other established name-resolution rules apply.
- **LA3:** A noncapturing lambda value is Copy and needs no destruction merely
  for being a function; its arguments may still own resources.
- **LA4:** Ordinary type/arity checks apply, including higher-order calls.
- **LA7:** Type output identifies each lambda by source expression/location
  and shows its function and parameter types.

**Assertions / test design:**

- Accept direct and higher-order calls: `g = (x) => x + 1`,
  `g(41)`, and `apply(g, 41)` with `apply(f, v) { return f(v); }`.
  Check `(int) -> int` for `g` and its lambda, and `int` for the parameter
  and both results.
- Accept a lambda using its own parameter. Reject capture of a surrounding
  local: assign `delta = 1`, then `g = (x) => x + delta`; assert the
  scope/capture cause.
- Reject both wrong arity (`g(1, 2)`) and an incompatible argument type, such as
  passing `[1]` to the integer lambda. Assert the call-type mismatch.
- Check that the lambda/function binding is Copy and generates no destruction
  merely for being a function. The accepted integer-only direct/higher-order
  example has zero named-owner destroys.
- Check `--ptype` for a separately identified lambda expression/location and
  its parameter and result types. Compare type structure, not generated names.
- Accept `if (flag) { f = (x) => x + 1; } else { f = (y) => y + 2; }`
  followed by `f(10)`. Check that
  the call graph includes both possible source-designated bodies; also check
  the higher-order call target in the `apply` example above.

## G12 — Function boundaries and lambda ownership effects

**Requirements:** SL9, SL10, LA5, LA6, LA8.

- **SL9:** A callee responsible for destroying owned elements needs sufficient
  cleanup knowledge.
- **SL10:** Borrowing retains the original owner; returning transfers ownership
  into the call result. Callees destroy only ownership they retain.
- **LA5–LA6:** Calls through lambda values resolve possible bodies and use
  their effects; ordinary ownership rules govern arguments/results even though
  the function value is Copy.
- **LA8:** Ownership output exposes relevant classes/destruction information;
  detailed traces record argument and result transfers.

**Assertions / test design:**

- Pair a generic consuming `size(q) { return #q; }` called with `Seq(int)`
  and with `Seq(own&int)`. Accept the former with cleanup in
  `size`; reject the latter for insufficient element-cleanup knowledge, not
  type incompatibility.
- Accept disposal when the callee has sufficient concrete element information
  from its body. For example, declare `type Box = Full(value);` and have
  `sizeKnown(q)` call a helper that matches the borrowed `q[0]` as `Full(x)`
  and reads integer `x`. This constrains `q` to `Seq(Box)`. Pass
  `[Full(1), Full(2)]` by value; require one destroy of `q` in `sizeKnown`
  and none for the moved caller binding. Check the recursive cleanup recipe.
- Accept `sizeBorrow(p) { return #*p; }` called with `&s` where
  `s = [alloc 1, alloc 2]`: the caller retains its
  owner and cleanup, while the borrowed parameter requires no destruction.
- Accept `pass(q) { return q; }` called as `t = pass(s)` for an owned-element
  sequence: the source and callee formal
  are not destroyed after transfer; the receiving binder is destroyed if
  retained. Check return-expression evaluation precedes exit cleanup.
- Apply the same distinctions through `apply(f, v) { return f(v); }`:
  `g = (q) => #q` consumes a `Seq(int)` argument, whereas `g = (q) => q`
  returns an owned-element sequence to the caller via `t = apply(g, s)`.
  Inspect call targets, summaries, and actual argument/result transfers.
- In the consuming case, require one sequence destroy in the lambda and none in
  main or the forwarding `apply`; in the returning case, require one destroy
  targeting `t` in main and none in `apply` or the returning lambda. Identify
  lambdas by source expression and location in output.
- Reject later use of the consumed source after the consuming lambda call. Also reject
  the generic consuming lambda called with owned elements when it lacks
  cleanup knowledge.
- Inspect `--pownership --constraint` for function/parameter/local classes,
  move and return transfers, named destruction counts, and SL18–SL19 plan
  details. Compare detailed cleanup output with the semantic plan used for
  destruction insertion.
