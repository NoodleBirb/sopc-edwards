# Deliverable 3 Requirements

Each construct section describes typing, ownership, and memory-safety behavior.
The invariant map identifies the machinery responsible for enforcing that
behavior. It describes intended enforcement, not a claim that the current
compiler already implements the extensions.

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
| SL6–SL8, LN5, SC5, RG2, BI5, LA3, LA6 | Cleanup accounts for retained owners, nested resources, and untransferred temporaries. | MoveAnalysis and DestructionPass use ownership/type information; extend temporary and sequence accounting. Runtime frees belong to Deliverable 4. |
| RG4, BI6 | Loops expose body, continuation, and repetition paths. | CFGBuilder constructs blocks/edges; CFGRenderer displays them. |
| SL14–SL17, IX7–IX9, LN6–LN7, SC6–SC7, RG5–RG6, BI7–BI9, LA7–LA8 | Views expose the specified types, classes, destroys, moves, and borrow provenance. | Existing inspection renderers consume the corresponding analysis results; extend their construct coverage. |

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
the element type shall be rejected. This restriction shall not be interpreted
as a blanket prohibition on polymorphic function schemes.

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
destruction. The semantic destruction behavior shall account for these
resources; implementing the actual runtime frees belongs to Deliverable 4.

**SL7 — Named-owner destruction.** A named sequence that remains Owned at
function exit shall receive a destruction operation before the function
returns, after computing the return value. A binding whose ownership has
transferred elsewhere, including through the return value, shall not destroy
that transferred value.

**SL8 — Unnamed sequence cleanup.** An unnamed sequence used by an operation
that does not retain or transfer its ownership shall be destroyed once that
operation no longer needs it. If ownership transfers into a binding, another
function, or a returned result, cleanup responsibility shall follow that
transfer.

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
after the length has been obtained.

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
source expression shall continue to apply independently.

**SC6 — Type inspection.** `--ptype` shall report `Seq(T)` for a binding
receiving a slice from a base of type `Seq(T)`.

**SC7 — Ownership inspection.** A named slice result shall be reported as Own
under SL16. Its destruction-summary and ownership-trace reporting shall follow
SL14 and SL17. Creating the slice shall not itself produce a move event for
the source sequence; other effects from evaluating its operands shall continue
to follow the existing rules.

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
