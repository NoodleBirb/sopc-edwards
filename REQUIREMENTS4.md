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

## Test Plan

Tests below are implemented; execution status is recorded in SOLUTION-D4.md.
All helpers are defined within their tests. Program paths are relative to
`test/system/`. Cases and coverage are also recorded in
[sop-d4-cases.json](test/system/sop-d4-cases.json).

Self-checks use `error` on assertion failure and finish with `Program output: 0`.
Program tests run with optimization enabled and disabled. The memory runner
repeats every successful program under ASan with leak detection enabled.

Runtime-failure tests must compile and link successfully, then exit nonzero with
the required category. Their complete pre-failure output must match the manifest;
post-failure output, crashes, or sanitizer reports fail the test. Runtime failures
run separately from leak checks because fatal-error unwinding is not a required
memory-lifecycle case.

### Sequence Literals and Length

| Test | Program or test artifact | Assertions | Primary requirements |
| --- | --- | --- | --- |
| T1 | [selftests/sop-d4-literal-values.top](test/system/selftests/sop-d4-literal-values.top) | Check length `3`, values `[10,20,30]`, and continued source use after length reads. | R3, R7, R9 |
| T2 | [selftests/sop-d4-nested-values.top](test/system/selftests/sop-d4-nested-values.top) | Check ragged row lengths and values; include a contextually typed empty literal in `[[],[1]]`. | R3, R6, R7, R10 |
| T3 | [selftests/sop-d4-empty-length.top](test/system/selftests/sop-d4-empty-length.top) | Check that an empty slice has length `0` and leaves its source unchanged. | R7, R9, R10 |
| T4 | [selftests/sop-d4-literal-evaluation.top](test/system/selftests/sop-d4-literal-evaluation.top) | Check evaluation prefixes `0,1,12`, final trace `123`, and resulting values `[1,2,3]`. | R3, R4 |
| T5 | [selftests/sop-d4-length-evaluation.top](test/system/selftests/sop-d4-length-evaluation.top) | A length operand increments a counter and returns a sequence; require one call and length `3`. | R7, R8, R63 |
| T6 | [selftests/sop-d4-literal-independence.top](test/system/selftests/sop-d4-literal-independence.top) | Evaluate the same literal twice; mutate the first and check both complete resulting sequences. | R5 |

### Sequence Indexing

| Test | Program or test artifact | Assertions | Primary requirements |
| --- | --- | --- | --- |
| T7 | [selftests/sop-d4-index-values.top](test/system/selftests/sop-d4-index-values.top) | Check all valid indices of `[10,20,30]`, including first/last, and retain source ownership. | R12, R13, R17 |
| T8 | [runtime/sop-d4-index-bounds.top](test/system/runtime/sop-d4-index-bounds.top) | Reject `-1`, `3`, `5`, and signed integer extremes for length `3`. | R1, R2, R12 |
| T9 | [runtime/sop-d4-empty-index.top](test/system/runtime/sop-d4-empty-index.top) | Reject every tested index of an empty sequence; still evaluate the index expression before failure. | R1, R2, R12, R14 |
| T10 | [selftests/sop-d4-index-evaluation.top](test/system/selftests/sop-d4-index-evaluation.top) | Require sequence/index trace `12`, once each, and selected value `20`. | R14, R64 |
| T11 | [selftests/sop-d4-index-borrow-int.top](test/system/selftests/sop-d4-index-borrow-int.top) | Mutate integer index `1` through `*p`; require `[10,99,30]` and unchanged length. | R16, R17 |
| T12 | [selftests/sop-d4-index-borrow-own.top](test/system/selftests/sop-d4-index-borrow-own.top) | Mutate an owned box through `**p`; require values `10,99` and continued source use. | R16, R17 |

### Sequence Slicing

| Test | Program or test artifact | Assertions | Primary requirements |
| --- | --- | --- | --- |
| T13 | [selftests/sop-d4-slice-values.top](test/system/selftests/sop-d4-slice-values.top) | Check explicit, omitted, and full slice bounds, result lengths, contents, and source preservation. | R18, R19, R20, R24, R25 |
| T14 | [selftests/sop-d4-slice-empty.top](test/system/selftests/sop-d4-slice-empty.top) | Check empty slices at start, middle, and end; copy an empty sequence. | R7, R10, R18, R19, R20 |
| T15 | [runtime/sop-d4-slice-bounds.top](test/system/runtime/sop-d4-slice-bounds.top) | Reject negative, reversed, excessive, degenerate-invalid, and extreme bounds. | R1, R2, R20, R22 |
| T16 | [selftests/sop-d4-slice-evaluation.top](test/system/selftests/sop-d4-slice-evaluation.top) | Require source/lower/upper trace `123`, once each, with result `[2,3]`. | R18, R21, R65 |
| T17 | [selftests/sop-d4-slice-independence.top](test/system/selftests/sop-d4-slice-independence.top) | Mutate source and slice separately; confirm isolation in both directions. | R23, R24 |
| T18 | [runtime/sop-d4-slice-failure-evaluation.top](test/system/runtime/sop-d4-slice-failure-evaluation.top) | Even invalid numeric bounds must evaluate all three operands before the bounds failure. | R1, R2, R20, R21, R22 |

### Range Loops

| Test | Program or test artifact | Assertions | Primary requirements |
| --- | --- | --- | --- |
| T19 | [selftests/sop-d4-range-values.top](test/system/selftests/sop-d4-range-values.top) | Check both directions, default step, exclusive end, equal endpoints, direction mismatch, and final binder. | R29, R31, R32, R33, R34, R36 |
| T20 | [selftests/sop-d4-range-evaluation.top](test/system/selftests/sop-d4-range-evaluation.top) | Check once-only range operands, saved bounds/step, and body binder changes independent of progression. | R28, R30, R34, R35, R36 |
| T21 | [selftests/sop-d4-range-overflow.top](test/system/selftests/sop-d4-range-overflow.top) | Check overflow in both directions, extreme steps, final binder, and continuation after the loop. | R38, R39 |
| T22 | [runtime/sop-d4-zero-step.top](test/system/runtime/sop-d4-zero-step.top) | Reject zero steps for nonempty, equal, and direction-mismatched endpoints before any body output. | R1, R2, R37 |

### Borrowed Sequence Iteration

| Test | Program or test artifact | Assertions | Primary requirements |
| --- | --- | --- | --- |
| T23 | [selftests/sop-d4-borrowed-copy.top](test/system/selftests/sop-d4-borrowed-copy.top) | Check visit order/count and that modifying a Copy binder does not write back. | R41, R42, R44, R45, R47, R48 |
| T24 | [selftests/sop-d4-borrowed-parameter.top](test/system/selftests/sop-d4-borrowed-parameter.top) | Iterate through borrowed parameters over nonempty and empty sequences without consuming them. | R41, R42, R43, R47, R48 |
| T25 | [selftests/sop-d4-borrowed-nested.top](test/system/selftests/sop-d4-borrowed-nested.top) | Visit nested rows through `borrow&T` binders and retain the outer owner. | R42, R46, R47, R48 |
| T26 | [selftests/sop-d4-borrowed-evaluation.top](test/system/selftests/sop-d4-borrowed-evaluation.top) | An indexed iterable increments a counter; require one evaluation and trace `123`. | R40, R42 |
| T27 | [selftests/sop-d4-borrowed-own.top](test/system/selftests/sop-d4-borrowed-own.top) | Read/mutate owned boxes through borrowed binders; retain the sequence and its elements. | R42, R46, R47, R48 |
| T28 | [selftests/sop-d4-borrowed-empty.top](test/system/selftests/sop-d4-borrowed-empty.top) | An empty borrowed loop preserves the prior Copy binder and executes no body. | R40, R43, R48 |

### Lambdas and Function Calls

| Test | Program or test artifact | Assertions | Primary requirements |
| --- | --- | --- | --- |
| T29 | [selftests/sop-d4-lambda-forms.top](test/system/selftests/sop-d4-lambda-forms.top) | Check immediate, stored, passed, returned, and sequence-selected lambdas; defer body effects until a call. | R49, R50, R51, R52, R53, R54 |
| T30 | [selftests/sop-d4-higher-order.top](test/system/selftests/sop-d4-higher-order.top) | Call one source-defined higher-order loop with addition and multiplication lambdas. | R54, R56, R57 |
| T31 | [selftests/sop-d4-call-evaluation.top](test/system/selftests/sop-d4-call-evaluation.top) | Require function-expression/argument trace `123`, once each, and correct subtraction result. | R51, R54, R55 |
| T32 | [selftests/sop-d4-lambda-consume.top](test/system/selftests/sop-d4-lambda-consume.top) | Consume a sequence through a lambda and higher-order forwarding call without double cleanup. | R56, R58, R59, R60, R68 |
| T33 | [selftests/sop-d4-lambda-transfer.top](test/system/selftests/sop-d4-lambda-transfer.top) | Return owned elements through a lambda and forwarding call; clean up only the receiving owner. | R56, R58, R59, R60, R68 |

### Memory Lifecycles

| Test | Program or test artifact | Assertions | Primary requirements |
| --- | --- | --- | --- |
| T34 | [selftests/sop-d4-memory-returned.top](test/system/selftests/sop-d4-memory-returned.top) | Return a sequence into a caller-owned binding and release it there. | R58, R59, R60, R66, R68 |
| T35 | [selftests/sop-d4-memory-moved.top](test/system/selftests/sop-d4-memory-moved.top) | Move a sequence between bindings and release only the final owner. | R58, R59, R60, R66, R68 |
| T36 | [selftests/sop-d4-memory-nested.top](test/system/selftests/sop-d4-memory-nested.top) | Recursively destroy a nested sequence of owned boxes. | R46, R58, R61, R66, R68 |
| T37 | [selftests/sop-d4-memory-sliced.top](test/system/selftests/sop-d4-memory-sliced.top) | Destroy source and copied slice independently; Copy elements require no destruction. | R23, R24, R58, R62, R66, R68 |
| T38 | [selftests/sop-d4-memory-unnamed.top](test/system/selftests/sop-d4-memory-unnamed.top) | Exercise unnamed length/index/slice values, a temporary slice source, and temporary borrowed-call lifetime. | R58, R63, R64, R65, R68 |
| T39 | [selftests/sop-d4-memory-owned-elements.top](test/system/selftests/sop-d4-memory-owned-elements.top) | Destroy owned boxes moved into sequence elements exactly once. | R58, R59, R60, R61, R66, R68 |
| T40 | [selftests/sop-d4-memory-sum-elements.top](test/system/selftests/sop-d4-memory-sum-elements.top) | Destroy sequence elements that are algebraic values containing owned boxes. | R46, R58, R61, R66, R68 |

### IR and Test Infrastructure

| Test | Program or test artifact | Assertions | Primary requirements |
| --- | --- | --- | --- |
| T41 | [SOPD4CodegenTest.cpp T41](test/unit/codegen/SOPD4CodegenTest.cpp) | Inspect allocation size/header contract, including a contextually typed empty literal. | R10, R11, R70, R80 |
| T42 | [SOPD4CodegenTest.cpp T42](test/unit/codegen/SOPD4CodegenTest.cpp) | Inspect the header read contributing to a length result. | R7, R70, R80 |
| T43 | [SOPD4CodegenTest.cpp T43](test/unit/codegen/SOPD4CodegenTest.cpp) | Require a success guard to dominate dynamic element access and an error path that cannot reach it. | R12, R15, R70, R80 |
| T44 | [SOPD4CodegenTest.cpp T44](test/unit/codegen/SOPD4CodegenTest.cpp) | Find a loop back edge in generated range control flow. | R31, R32, R33, R70, R80 |
| T45 | [SOPD4CodegenTest.cpp T45](test/unit/codegen/SOPD4CodegenTest.cpp) | Require return-value computation before cleanup and cleanup before return. | R66, R67, R68, R70, R80 |
| T46 | [SOPD4CodegenTest.cpp T46](test/unit/codegen/SOPD4CodegenTest.cpp) | Find the emitted lambda/table entry and indirect call; require no function-environment allocation. | R49, R50, R51, R52, R54, R70, R80 |
| T47 | [SOPD4CodegenTest.cpp T47](test/unit/codegen/SOPD4CodegenTest.cpp) | Check ASan read/write instrumentation with optimization enabled and disabled. | R70, R73 |
| T48 | [SOPD4CodegenTest.cpp T48](test/unit/codegen/SOPD4CodegenTest.cpp) | Find dynamic slice copying and allocation linear in `hi - lo`. | R18, R26, R27, R70, R80 |
| T49 | [test/system/test_sop_d4_runner.py](test/system/test_sop_d4_runner.py) | Reject compiler/link failures, crashes, missing categories/effects, later output, and sanitizer reports as false positives. | R1, R2 |
| T50 | [test/system/run.py](test/system/run.py) | Run existing TOP/D3 suites unchanged and review scope/non-goal preservation. | R76, R77, R78, R79 |

### Additional Implementation Regressions

| Test | Program or test artifact | Assertions | Primary requirements |
| --- | --- | --- | --- |
| T51 | [selftests/sop-d4-generic-iteration.top](test/system/selftests/sop-d4-generic-iteration.top) | One generic loop handles Copy and Own elements; verify named forwarding and a lambda relay. | R42, R44, R46, R47, R48, R54, R56, R57 |
| T52 | [selftests/sop-d4-temporary-loop.top](test/system/selftests/sop-d4-temporary-loop.top) | Keep a temporary nested owner alive throughout borrowed iteration; repeat temporary length/slice cleanup inside a range. | R40, R46, R48, R58, R61, R63, R65, R68 |

### Memory-Safety Matrix

Release points are required behavior; sanitizer results remain separate evidence.

| Lifecycle | Test | Required release point |
| --- | --- | --- |
| Returned | [T34](test/system/selftests/sop-d4-memory-returned.top) | Receiver's exit, after evaluating its return value; no release in the returning producer. |
| Moved | [T35](test/system/selftests/sop-d4-memory-moved.top) | Final owner's exit; no release by the moved-from binding. |
| Nested | [T36](test/system/selftests/sop-d4-memory-nested.top) | Destroy owned boxes, then rows, then outer storage. |
| Copied by a slice | [T37](test/system/selftests/sop-d4-memory-sliced.top) | Source and slice each release their own storage at their planned endpoints. |
| Never named | [T38](test/system/selftests/sop-d4-memory-unnamed.top) | After length/Copy-element reads, after slice copying, or after the receiving borrowed call. |
| Owning its elements | [T39](test/system/selftests/sop-d4-memory-owned-elements.top), [T40](test/system/selftests/sop-d4-memory-sum-elements.top) | Recursively destroy owned boxes/algebraic payloads before sequence storage. |

### Requirement Traceability

“All programs” means T1–T40 and T51–T52, including both optimization modes.
“Memory runner” means all successful program cases under sanitizers; it excludes
T8, T9, T15, T18, and T22. Scope and non-goals also require review alongside the unchanged D3
regressions; an execution test alone does not prove the absence of new features.

| Requirement | Tests |
| --- | --- |
| R1 | [T8](test/system/runtime/sop-d4-index-bounds.top), [T9](test/system/runtime/sop-d4-empty-index.top), [T15](test/system/runtime/sop-d4-slice-bounds.top), [T18](test/system/runtime/sop-d4-slice-failure-evaluation.top), [T22](test/system/runtime/sop-d4-zero-step.top), [T49](test/system/test_sop_d4_runner.py) |
| R2 | [T8](test/system/runtime/sop-d4-index-bounds.top), [T9](test/system/runtime/sop-d4-empty-index.top), [T15](test/system/runtime/sop-d4-slice-bounds.top), [T18](test/system/runtime/sop-d4-slice-failure-evaluation.top), [T22](test/system/runtime/sop-d4-zero-step.top), [T49](test/system/test_sop_d4_runner.py) |
| R3 | [T1](test/system/selftests/sop-d4-literal-values.top), [T2](test/system/selftests/sop-d4-nested-values.top), [T4](test/system/selftests/sop-d4-literal-evaluation.top) |
| R4 | [T4](test/system/selftests/sop-d4-literal-evaluation.top) |
| R5 | [T6](test/system/selftests/sop-d4-literal-independence.top) |
| R6 | [T2](test/system/selftests/sop-d4-nested-values.top) |
| R7 | [T1](test/system/selftests/sop-d4-literal-values.top), [T2](test/system/selftests/sop-d4-nested-values.top), [T3](test/system/selftests/sop-d4-empty-length.top), [T5](test/system/selftests/sop-d4-length-evaluation.top), [T14](test/system/selftests/sop-d4-slice-empty.top), [T42](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R8 | [T5](test/system/selftests/sop-d4-length-evaluation.top) |
| R9 | [T1](test/system/selftests/sop-d4-literal-values.top), [T3](test/system/selftests/sop-d4-empty-length.top) |
| R10 | [T2](test/system/selftests/sop-d4-nested-values.top), [T3](test/system/selftests/sop-d4-empty-length.top), [T14](test/system/selftests/sop-d4-slice-empty.top), [T41](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R11 | [T41](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R12 | [T7](test/system/selftests/sop-d4-index-values.top), [T8](test/system/runtime/sop-d4-index-bounds.top), [T9](test/system/runtime/sop-d4-empty-index.top), [T43](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R13 | [T7](test/system/selftests/sop-d4-index-values.top) |
| R14 | [T9](test/system/runtime/sop-d4-empty-index.top), [T10](test/system/selftests/sop-d4-index-evaluation.top) |
| R15 | [T43](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R16 | [T11](test/system/selftests/sop-d4-index-borrow-int.top), [T12](test/system/selftests/sop-d4-index-borrow-own.top) |
| R17 | [T7](test/system/selftests/sop-d4-index-values.top), [T11](test/system/selftests/sop-d4-index-borrow-int.top), [T12](test/system/selftests/sop-d4-index-borrow-own.top) |
| R18 | [T13](test/system/selftests/sop-d4-slice-values.top), [T14](test/system/selftests/sop-d4-slice-empty.top), [T16](test/system/selftests/sop-d4-slice-evaluation.top), [T48](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R19 | [T13](test/system/selftests/sop-d4-slice-values.top), [T14](test/system/selftests/sop-d4-slice-empty.top) |
| R20 | [T13](test/system/selftests/sop-d4-slice-values.top), [T14](test/system/selftests/sop-d4-slice-empty.top), [T15](test/system/runtime/sop-d4-slice-bounds.top), [T18](test/system/runtime/sop-d4-slice-failure-evaluation.top) |
| R21 | [T16](test/system/selftests/sop-d4-slice-evaluation.top), [T18](test/system/runtime/sop-d4-slice-failure-evaluation.top) |
| R22 | [T15](test/system/runtime/sop-d4-slice-bounds.top), [T18](test/system/runtime/sop-d4-slice-failure-evaluation.top) |
| R23 | [T17](test/system/selftests/sop-d4-slice-independence.top), [T37](test/system/selftests/sop-d4-memory-sliced.top) |
| R24 | [T13](test/system/selftests/sop-d4-slice-values.top), [T17](test/system/selftests/sop-d4-slice-independence.top), [T37](test/system/selftests/sop-d4-memory-sliced.top) |
| R25 | [T13](test/system/selftests/sop-d4-slice-values.top) |
| R26 | [T48](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R27 | [T48](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R28 | [T20](test/system/selftests/sop-d4-range-evaluation.top) |
| R29 | [T19](test/system/selftests/sop-d4-range-values.top) |
| R30 | [T20](test/system/selftests/sop-d4-range-evaluation.top) |
| R31 | [T19](test/system/selftests/sop-d4-range-values.top), [T44](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R32 | [T19](test/system/selftests/sop-d4-range-values.top), [T44](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R33 | [T19](test/system/selftests/sop-d4-range-values.top), [T44](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R34 | [T19](test/system/selftests/sop-d4-range-values.top), [T20](test/system/selftests/sop-d4-range-evaluation.top) |
| R35 | [T20](test/system/selftests/sop-d4-range-evaluation.top) |
| R36 | [T19](test/system/selftests/sop-d4-range-values.top), [T20](test/system/selftests/sop-d4-range-evaluation.top) |
| R37 | [T22](test/system/runtime/sop-d4-zero-step.top) |
| R38 | [T21](test/system/selftests/sop-d4-range-overflow.top) |
| R39 | [T21](test/system/selftests/sop-d4-range-overflow.top) |
| R40 | [T26](test/system/selftests/sop-d4-borrowed-evaluation.top), [T28](test/system/selftests/sop-d4-borrowed-empty.top), [T52](test/system/selftests/sop-d4-temporary-loop.top) |
| R41 | [T23](test/system/selftests/sop-d4-borrowed-copy.top), [T24](test/system/selftests/sop-d4-borrowed-parameter.top) |
| R42 | [T23](test/system/selftests/sop-d4-borrowed-copy.top), [T24](test/system/selftests/sop-d4-borrowed-parameter.top), [T25](test/system/selftests/sop-d4-borrowed-nested.top), [T26](test/system/selftests/sop-d4-borrowed-evaluation.top), [T27](test/system/selftests/sop-d4-borrowed-own.top), [T51](test/system/selftests/sop-d4-generic-iteration.top) |
| R43 | [T24](test/system/selftests/sop-d4-borrowed-parameter.top), [T28](test/system/selftests/sop-d4-borrowed-empty.top) |
| R44 | [T23](test/system/selftests/sop-d4-borrowed-copy.top), [T51](test/system/selftests/sop-d4-generic-iteration.top) |
| R45 | [T23](test/system/selftests/sop-d4-borrowed-copy.top) |
| R46 | [T25](test/system/selftests/sop-d4-borrowed-nested.top), [T27](test/system/selftests/sop-d4-borrowed-own.top), [T36](test/system/selftests/sop-d4-memory-nested.top), [T40](test/system/selftests/sop-d4-memory-sum-elements.top), [T51](test/system/selftests/sop-d4-generic-iteration.top), [T52](test/system/selftests/sop-d4-temporary-loop.top) |
| R47 | [T23](test/system/selftests/sop-d4-borrowed-copy.top), [T24](test/system/selftests/sop-d4-borrowed-parameter.top), [T25](test/system/selftests/sop-d4-borrowed-nested.top), [T27](test/system/selftests/sop-d4-borrowed-own.top), [T51](test/system/selftests/sop-d4-generic-iteration.top) |
| R48 | [T23](test/system/selftests/sop-d4-borrowed-copy.top), [T24](test/system/selftests/sop-d4-borrowed-parameter.top), [T25](test/system/selftests/sop-d4-borrowed-nested.top), [T27](test/system/selftests/sop-d4-borrowed-own.top), [T28](test/system/selftests/sop-d4-borrowed-empty.top), [T51](test/system/selftests/sop-d4-generic-iteration.top), [T52](test/system/selftests/sop-d4-temporary-loop.top) |
| R49 | [T29](test/system/selftests/sop-d4-lambda-forms.top), [T46](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R50 | [T29](test/system/selftests/sop-d4-lambda-forms.top), [T46](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R51 | [T29](test/system/selftests/sop-d4-lambda-forms.top), [T31](test/system/selftests/sop-d4-call-evaluation.top), [T46](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R52 | [T29](test/system/selftests/sop-d4-lambda-forms.top), [T46](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R53 | [T29](test/system/selftests/sop-d4-lambda-forms.top) |
| R54 | [T29](test/system/selftests/sop-d4-lambda-forms.top), [T30](test/system/selftests/sop-d4-higher-order.top), [T31](test/system/selftests/sop-d4-call-evaluation.top), [T46](test/unit/codegen/SOPD4CodegenTest.cpp), [T51](test/system/selftests/sop-d4-generic-iteration.top) |
| R55 | [T31](test/system/selftests/sop-d4-call-evaluation.top) |
| R56 | [T30](test/system/selftests/sop-d4-higher-order.top), [T32](test/system/selftests/sop-d4-lambda-consume.top), [T33](test/system/selftests/sop-d4-lambda-transfer.top), [T51](test/system/selftests/sop-d4-generic-iteration.top) |
| R57 | [T30](test/system/selftests/sop-d4-higher-order.top), [T51](test/system/selftests/sop-d4-generic-iteration.top) |
| R58 | [T32](test/system/selftests/sop-d4-lambda-consume.top), [T33](test/system/selftests/sop-d4-lambda-transfer.top), [T34](test/system/selftests/sop-d4-memory-returned.top), [T35](test/system/selftests/sop-d4-memory-moved.top), [T36](test/system/selftests/sop-d4-memory-nested.top), [T37](test/system/selftests/sop-d4-memory-sliced.top), [T38](test/system/selftests/sop-d4-memory-unnamed.top), [T39](test/system/selftests/sop-d4-memory-owned-elements.top), [T40](test/system/selftests/sop-d4-memory-sum-elements.top), [T52](test/system/selftests/sop-d4-temporary-loop.top) |
| R59 | [T32](test/system/selftests/sop-d4-lambda-consume.top), [T33](test/system/selftests/sop-d4-lambda-transfer.top), [T34](test/system/selftests/sop-d4-memory-returned.top), [T35](test/system/selftests/sop-d4-memory-moved.top), [T39](test/system/selftests/sop-d4-memory-owned-elements.top) |
| R60 | [T32](test/system/selftests/sop-d4-lambda-consume.top), [T33](test/system/selftests/sop-d4-lambda-transfer.top), [T34](test/system/selftests/sop-d4-memory-returned.top), [T35](test/system/selftests/sop-d4-memory-moved.top), [T39](test/system/selftests/sop-d4-memory-owned-elements.top) |
| R61 | [T36](test/system/selftests/sop-d4-memory-nested.top), [T39](test/system/selftests/sop-d4-memory-owned-elements.top), [T40](test/system/selftests/sop-d4-memory-sum-elements.top), [T52](test/system/selftests/sop-d4-temporary-loop.top) |
| R62 | [T37](test/system/selftests/sop-d4-memory-sliced.top) |
| R63 | [T5](test/system/selftests/sop-d4-length-evaluation.top), [T38](test/system/selftests/sop-d4-memory-unnamed.top), [T52](test/system/selftests/sop-d4-temporary-loop.top) |
| R64 | [T10](test/system/selftests/sop-d4-index-evaluation.top), [T38](test/system/selftests/sop-d4-memory-unnamed.top) |
| R65 | [T16](test/system/selftests/sop-d4-slice-evaluation.top), [T38](test/system/selftests/sop-d4-memory-unnamed.top), [T52](test/system/selftests/sop-d4-temporary-loop.top) |
| R66 | [T34](test/system/selftests/sop-d4-memory-returned.top), [T35](test/system/selftests/sop-d4-memory-moved.top), [T36](test/system/selftests/sop-d4-memory-nested.top), [T37](test/system/selftests/sop-d4-memory-sliced.top), [T39](test/system/selftests/sop-d4-memory-owned-elements.top), [T40](test/system/selftests/sop-d4-memory-sum-elements.top), [T45](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R67 | [T45](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R68 | [T32](test/system/selftests/sop-d4-lambda-consume.top), [T33](test/system/selftests/sop-d4-lambda-transfer.top), [T34](test/system/selftests/sop-d4-memory-returned.top), [T35](test/system/selftests/sop-d4-memory-moved.top), [T36](test/system/selftests/sop-d4-memory-nested.top), [T37](test/system/selftests/sop-d4-memory-sliced.top), [T38](test/system/selftests/sop-d4-memory-unnamed.top), [T39](test/system/selftests/sop-d4-memory-owned-elements.top), [T40](test/system/selftests/sop-d4-memory-sum-elements.top), [T45](test/unit/codegen/SOPD4CodegenTest.cpp), [T52](test/system/selftests/sop-d4-temporary-loop.top) |
| R69 | [All programs](test/system/sop-d4-cases.json) |
| R70 | [T41](test/unit/codegen/SOPD4CodegenTest.cpp), [T42](test/unit/codegen/SOPD4CodegenTest.cpp), [T43](test/unit/codegen/SOPD4CodegenTest.cpp), [T44](test/unit/codegen/SOPD4CodegenTest.cpp), [T45](test/unit/codegen/SOPD4CodegenTest.cpp), [T46](test/unit/codegen/SOPD4CodegenTest.cpp), [T47](test/unit/codegen/SOPD4CodegenTest.cpp), [T48](test/unit/codegen/SOPD4CodegenTest.cpp) |
| R71 | [All programs](test/system/sop-d4-cases.json) |
| R72 | [All programs](test/system/sop-d4-cases.json) |
| R73 | [T47](test/unit/codegen/SOPD4CodegenTest.cpp), [Memory runner](bin/run-sop-memory-tests.sh) |
| R74 | [Memory runner](bin/run-sop-memory-tests.sh) |
| R75 | [Memory runner](bin/run-sop-memory-tests.sh) |
| R76 | [T50](test/system/run.py) |
| R77 | [T50](test/system/run.py) |
| R78 | [T50](test/system/run.py) |
| R79 | [T50](test/system/run.py) |
| R80 | [T41](test/unit/codegen/SOPD4CodegenTest.cpp), [T42](test/unit/codegen/SOPD4CodegenTest.cpp), [T43](test/unit/codegen/SOPD4CodegenTest.cpp), [T44](test/unit/codegen/SOPD4CodegenTest.cpp), [T45](test/unit/codegen/SOPD4CodegenTest.cpp), [T46](test/unit/codegen/SOPD4CodegenTest.cpp), [T48](test/unit/codegen/SOPD4CodegenTest.cpp) |

### Running the Tests

Build `topc`, `top_rtlib`, and `codegen_unit_tests` with the matching LLVM toolchain.
Set `TOPCLANG` to its clang executable. `TOPC` and `RTLIB` may select an alternate build.

```bash
bin/run-sop-tests.sh --check
bin/run-sop-tests.sh --semantic-only
bin/run-sop-tests.sh --junit /tmp/sop-d4.xml
bin/run-sop-memory-tests.sh --junit /tmp/sop-d4-memory.xml
ctest --test-dir build -R '^SOP[ _]D4' --output-on-failure
python3 test/system/run.py --exclude-sop-d4
```

`bin/runtests.sh` includes the new self-checks, runtime-failure cases, and CTest
checks. `--exclude-sop-d4` isolates the preexisting system regressions.
