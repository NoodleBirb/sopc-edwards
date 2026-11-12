# Deliverable 4 Runtime Requirements

Applies to semantically accepted SOP programs. Indices are zero-based; `n`
denotes sequence length. Runtime errors follow the rules below.

## Runtime Errors

**R1.** A runtime error shall report its category and terminate the program with a nonzero exit status.

**R2.** After runtime failure, no later operand, statement, or output shall execute.

## Sequence Literals

**R3.** A literal of `n` expressions shall produce an owned sequence of length `n`, with results stored in source order.

**R4.** Each reached literal element expression shall be evaluated exactly once, from left to right.

**R5.** Each literal evaluation shall create fresh sequence storage.

**R6.** Nested elements shall remain distinct sequence values with their own lengths, including unequal row lengths.

## Sequence Length

**R7.** `#s` shall return its outer element count, including `0` for an empty sequence.

**R8.** A length expression shall evaluate its operand exactly once.

**R9.** Reading length shall preserve the source's contents and ownership.

## Chosen Sequence Representation

**R10.** A length-`n` sequence shall occupy `n + 1` 64-bit words: its length followed by ordered elements.

**R11.** Code generation and runtime helpers shall share this layout and TOP's `i64` address representation.

## Sequence Indexing

**R12.** Indexing shall accept exactly `0 <= i < n`; other indices shall fail with `OutOfBounds`.

**R13.** A valid value-index operation shall return the element at zero-based index `i`.

**R14.** Indexing shall evaluate its sequence expression, then its index expression, exactly once each.

**R15.** Bounds shall be checked before reading or borrowing the indexed element.

**R16.** Borrowed indexing shall return a non-owning reference to the element's storage slot.

**R17.** Indexing shall not consume sequence or element ownership.

## Sequence Slicing

**R18.** A valid slice shall copy `[lo, hi)` in order into an owned sequence of length `hi - lo`.

**R19.** Omitted slice bounds shall default to `lo = 0` and `hi = n`.

**R20.** Slicing shall accept exactly `0 <= lo <= hi <= n`; other bounds shall fail with `OutOfBounds`.

**R21.** Slicing shall evaluate its source, supplied lower bound, and supplied upper bound once each, in that order.

**R22.** Slice bounds shall be validated before copying any elements.

**R23.** Each slice shall have fresh storage independent of its source.

**R24.** Slicing shall not consume source ownership.

**R25.** Slicing shall retain D3's restriction to Copy-class elements.

**R26.** Copying a slice of `k` elements shall require O(k) element-copy work.

**R27.** A slice of `k` elements shall require O(k + 1) additional sequence storage.

## Range Loops

**R28.** Range start, end, and supplied step shall be evaluated once each, in that order, before iteration.

**R29.** An omitted range step shall default to `+1`.

**R30.** Iteration shall use saved bounds and step, unaffected by body changes to their source variables.

**R31.** The internal counter shall start at the evaluated start value.

**R32.** For nonzero step, iteration shall continue while `counter < end` if positive, or `counter > end` if negative.

**R33.** Between iterations, the internal counter shall advance by the saved step.

**R34.** Only a successful range test shall assign the counter to the binder, immediately before the body.

**R35.** Body assignments to the binder shall not change the internal counter.

**R36.** After the loop, the binder shall retain its last body value, or its previous value if no iteration ran.

**R37.** A zero step shall fail with `ZeroStep` before binder assignment or body execution, including for otherwise empty ranges.

**R38.** Advancement outside the signed 64-bit range shall end the loop before using a wrapped counter.

**R39.** Overflow completion shall continue after the loop without another binder assignment.

## Borrowed Sequence Iteration

**R40.** A borrowed sequence loop shall evaluate its iterable expression exactly once.

**R41.** Borrowed iteration shall support direct header borrows and already-borrowed sequence expressions.

**R42.** The body shall execute once per element in increasing index order.

**R43.** An empty sequence shall execute neither the body nor a binder assignment.

**R44.** A Copy element shall populate the binder with its current value before each body execution.

**R45.** Assigning a Copy binder shall not write back to its source element.

**R46.** An Own element of type `T` shall populate the binder as `borrow&T`, referring to the element slot.

**R47.** Borrowed iteration shall transfer neither sequence nor element ownership.

**R48.** Iteration completion shall not destroy its borrowed binder, referent, or source sequence.

## Lambdas and Function Calls

**R49.** A lambda expression shall produce its materialized function's `i64` function-table index.

**R50.** Each materialized lambda function shall be emitted and registered in the function table.

**R51.** A lambda body shall execute only when its function is called.

**R52.** Function values shall be Copy and require no heap allocation or destruction themselves.

**R53.** Function values shall support calls, assignment, passing, returning, and storage in sequences.

**R54.** Calls shall invoke the body identified by the evaluated function value.

**R55.** Calls shall evaluate the function expression, then arguments left to right, exactly once each.

**R56.** Indirect calls shall follow the same validated ownership and cleanup contracts as named calls.

**R57.** Source-defined higher-order functions shall execute their source bodies without name-specific compiler treatment.

## Memory Cleanup and Ownership Transfers

**R58.** On normal completion, each owned allocation shall have been released exactly once.

**R59.** Moves and owning returns shall transfer the existing allocation and cleanup responsibility to the recipient.

**R60.** A previous owner shall not destroy an allocation after transferring it.

**R61.** Sequence destruction shall recursively destroy Own elements before freeing the container.

**R62.** Copy elements shall require no individual destruction.

**R63.** An untransferred temporary sequence used for length shall be released after reading its length.

**R64.** An untransferred temporary sequence used for a Copy-element read shall be released after the element load.

**R65.** An untransferred temporary slice source shall be released after copying finishes.

**R66.** Retained local owners shall be destroyed after return-expression evaluation and before the return instruction.

**R67.** Return values shall remain available across cleanup calls.

**R68.** Cleanup and transfers shall follow D3's validated destruction plan and type-directed recipes.

## LLVM Generation and Runtime Integration

**R69.** Every semantically accepted SOP program shall compile and link with the runtime through the supported LLVM toolchain.

**R70.** Generated LLVM modules shall pass LLVM verification.

**R71.** Generated calls shall match runtime definitions' calling interfaces.

**R72.** Optimized and unoptimized code shall satisfy the same runtime requirements.

**R73.** Sanitizer mode shall instrument new SOP memory accesses and cleanup helpers, with optimization enabled or disabled.

**R74.** Sanitized executables shall link with the required sanitizer runtime.

**R75.** Valid memory-lifecycle cases shall produce no AddressSanitizer or LeakSanitizer reports with leak detection enabled.

## TOP Compatibility

**R76.** Existing TOP programs and supported driver and inspection options shall retain their established behavior.

**R77.** All preexisting unit and system tests shall continue to pass.

## Scope and Non-goals

**R78.** Semantic-analysis changes shall be limited to fixes needed for intended SOP behavior.

**R79.** New syntax, types, ownership features, closure capture, owned-element cloning, and language-level exceptions are out of scope.

**R80.** Diagnostic wording beyond required categories, generated names, instruction shapes, allocator choice, and helper organization shall remain implementation-defined.
