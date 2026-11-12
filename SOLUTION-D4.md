# Deliverable 4: Code Generation and Runtime Design

This note records the design decisions made while developing
[REQUIREMENTS4.md](REQUIREMENTS4.md). The test suite, traceability table, and
memory-safety matrix are implemented. D4 compiler lowering is still pending.
The walkthrough below describes intended lowering, not IR captured from a
completed implementation. Actual test results are recorded at the end.

## Sequence Representation

Requirements: R3–R11.

A sequence uses one heap allocation containing its length followed by its
elements. Each entry is one 64-bit word. For a sequence of length `n`, the
allocation occupies `8 * (n + 1)` bytes, excluding allocator bookkeeping.

| Byte offset from the allocation address | Contents |
| --- | --- |
| `0` | Length `n` |
| `8 * (i + 1)` | Element at zero-based index `i` |

The sequence value travels through existing TOP code as an `i64` holding the
allocation address. Code generation converts it to `ptr` when accessing memory
and converts allocated pointers back to `i64` when producing source values.
An empty sequence still has one word containing length `0`.

For `[[1, 2], [3]]`, the local variable holds the address `P`:

```text
local slot for s: P

P: outer allocation, 24 bytes
    [ length 2 | Q | R ]
                 |   |
                 |   +----> R: [ length 1 | 3 ]       16 bytes
                 +--------> Q: [ length 2 | 1 | 2 ]   24 bytes
```

The three allocations total 64 bytes. Each row keeps its own length, so rows
need not have equal lengths. Outer element slots hold row addresses rather than
flattened contents. Static element types determine whether a word represents an
integer, function-table index, or owned address.

This layout keeps the header and element storage together, with no separate
descriptor-to-data indirection. The compiler and any runtime helpers must agree
on it. Allocator choice and helper organization remain implementation details.
Literal element expressions are evaluated once, left to right, and each literal
execution creates independent sequence storage.

## Bounds Checks and Slices

Requirements: R12–R27.

Indexing first evaluates the sequence, then the index, once each. Its validity
condition is `0 <= index < length`. Generated control flow branches to a success
block or an error block before accessing the element. Value indexing loads the
element; borrowed indexing produces its checked storage address without taking
ownership. When generating an address, the visitor must still evaluate the
sequence and index operands in value mode.

A failed check reports the `OutOfBounds` category and terminates with nonzero
status. The error block calls a nonreturning error path and ends in `unreachable`.
No invalid access or subsequent source operation executes. Exact diagnostic
wording beyond the category is not fixed.

A slice evaluates its source, supplied lower bound, and supplied upper bound
once in that order. Missing bounds default to `0` and the source length. Validity
requires `0 <= lo <= hi <= length`. The result contains indices `lo` through
`hi - 1`, with length `k = hi - lo`.

The slice gets independent storage and copies its selected Copy elements;
mutations of the source and result do not affect one another. Existing D3 rules
reject slicing Own elements, so this is not recursive cloning. Copy work is
O(k), and result storage is `8 * (k + 1)` bytes. Empty slices have a zero length
header and perform no element loads. Invalid bounds fail before copying.

## Range and Borrowed Sequence Loops

Requirements: R28–R48.

A range evaluates start, end, and supplied step once, left to right, before
iteration. The default step is `+1`. The loop keeps a private counter separate
from the source binder:

```text
evaluate and retain start, end, step
reject a zero step with ZeroStep and terminate the program
counter = start
while (step > 0 and counter < end) or (step < 0 and counter > end):
    binder = counter
    execute body
    advance counter safely; finish the loop if advancement overflows
continue with the statement after the loop
```

The end is exclusive. Equal endpoints and direction mismatches produce no body
execution or binder assignment. Assignments to the binder or source bound
variables do not change the saved counter progression. Afterward, the binder
retains the value left by the final body execution, or its previous value for
an empty loop.

Counter overflow ends only the loop, rather than using a wrapped counter or
terminating the program. LLVM's signed-add-with-overflow intrinsic is one suitable
mechanism; the required behavior does not depend on that specific mechanism.

Borrowed sequence iteration evaluates its iterable once and visits increasing
indices. Copy elements are loaded into the binder. Under the current D3 contract,
Own elements give a `borrow&T` binder referring to the element slot. Neither
the binder nor loop completion takes over the sequence owner's cleanup.
Resources created by the body follow their semantic cleanup and transfer plan.

**Chosen binder contract:** D3's BI2/BI7 and R46 deliberately make every
Own-element borrowed-loop binder `borrow&T`. For example, `borrow&own&int` is a
non-owning reference to a slot containing an owned integer pointer. The outer
borrow already encodes non-owning access, so the binder does not need to acquire
ownership or become an owning binding. Ordinary owned bindings are unaffected.

**Remaining source compatibility:** page 7 of the D4 handout describes
element-typed non-owning binders and forwards them with an additional explicit
borrow. A fresh D3 build rejects the handout's owned-row forwarding example
because it adds a borrow to an already-borrowed binder. T25 and T27 use the
selected D3 convention and pass the binder directly. The chosen `borrow&T`
contract remains unchanged; supporting the handout's additional-borrow call
forms remains an integration issue. Semantic fixes needed for intended SOP
behavior are permitted by R78.

## Destruction and Unnamed Sequences

Requirements: R58–R68, together with the loop and call ownership contracts.

Code generation consumes D3's validated destruction plan, including its cleanup
points and type-directed recipes. It does not infer responsibility from where an
allocation happens to appear. Every owned allocation is released exactly once
at its required lifetime endpoint on normal completion.

Moving or returning an owned value transfers its address and cleanup
responsibility. The previous owner does not free it. Reading a borrowed value
does not take responsibility for freeing its referent.

For `Seq(Copy)`, destruction frees only the containing allocation. For Own
elements, generated code traverses the runtime elements, recursively destroys
them according to their types, and then frees the container. Thus the two rows
in the diagram are freed before their outer sequence. The compiler decides the
cleanup strategy from types; generated loops traverse the actual runtime values.

| Use of an unnamed sequence | Required cleanup point |
| --- | --- |
| Read its length | After obtaining the integer length |
| Read a Copy element | After loading the element value |
| Use it as a slice source | After copying the selected span |
| Transfer its result into a binding, argument, or return | Responsibility follows the recipient; no extra temporary free |

A slice retained by a variable has its own lifetime, independent of a temporary
source. A retained named owner is destroyed at its planned function exit, after
return-expression evaluation and before the return instruction. A sequence
returned by ownership transfer is not destroyed by the returning function.

If cleanup helpers are generated, their declarations must be cached before
building recursive bodies. Helper construction saves and restores the builder's
insertion point so subsequent caller instructions stay in the caller. Building
a helper during compilation does not execute its runtime cleanup.

## Lambda Values and Calls

Requirements: R49–R57.

Existing lambda preparation materializes noncapturing lambdas as synthetic
functions. D4 must emit/register those functions and make evaluating the lambda
produce its `i64` function-table index. Function values are Copy and need no
heap environment or destruction solely for being functions.

A call evaluates the function expression, then its arguments left to right,
once each. The existing call mechanism uses the resulting index to load a
function pointer from the table and invoke it. This supports immediate calls,
variables, arguments, returned functions, and functions selected from sequences.
Argument and result ownership follow the ordinary validated call contracts.
Source-defined higher-order functions use their ordinary bodies and calls;
no particular user-defined helper receives special compiler treatment.

## Planned IR Walkthrough

Related requirements: R3–R17, R28–R39, R58, R66–R72.

The following complete source program is proposed for the required final IR
walkthrough:

```text
main(index) {
    var s, i, total;
    s = [10, 20, 30];
    total = 0;
    for (i : 0 .. #s) {
        total = total + s[i];
    }
    return total + s[index];
}
```

For input `1`, its intended return value is `80`: the loop totals `60`, then the
return expression adds `20`. An invalid final index must fail before its element
load. The table describes intended block responsibilities, not captured IR or
mandatory block names:

| Planned block | Source and generated operations |
| --- | --- |
| Initialization | Allocate four words for the literal; store length and elements; initialize total; evaluate and save the range values |
| Range test | Compare the private counter with the exclusive saved end; choose body or continuation |
| Body/index check | Assign the binder; evaluate `s[i]` operands and validate the index |
| Body/index success | Load the element and update total |
| Counter advance | Advance safely; return to the test or finish on overflow |
| Return/index check | Validate `index` for the final `s[index]` |
| Return/index success | Load the element and compute the complete return value |
| Cleanup/return | Free the retained sequence after computing the result, then return it |
| Bounds failure | Report `OutOfBounds`, terminate, and end the IR block with `unreachable` |

**Required completion:** after implementation, capture this program's actual IR
with optimization disabled, verify it with LLVM, and replace or supplement this
plan with the emitted blocks and instructions. Identify the allocation, length
read, checks, and release in that captured output. Constant-known checks may
simplify, so record the actual output rather than assume this exact block layout.

## Validation and TOP Regression Evidence

Requirements: R69–R80 and the construct-specific requirements.

The suite contains 40 program fixtures, eight Catch2 IR cases, a runtime-oracle
test module, and existing-regression coverage. T1–T50, links for R1–R80, and all
six required lifecycle rows are recorded in REQUIREMENTS4.md. The machine-readable
manifest is `test/system/sop-d4-cases.json`; its consistency check verifies the
documented source paths, test identifiers, and requirement links.

`bin/run-sop-tests.sh` separates compilation, linking, and execution. A runtime
failure passes only with the expected category, nonzero status, and exact
pre-failure output. Compiler rejection, signal termination, later output, and
sanitizer reports fail. `bin/run-sop-memory-tests.sh` reruns all 35 valid programs
with ASan and leak detection, in both optimization modes. The five failure
programs contain 23 parameterized input cases and run separately from normal
lifecycle checks.

### Results Recorded 2026-11-12

The compiler source base is `2064c7e`; the new test changes are additional work.
Validation used an isolated Debug build at `/tmp/sopc-d4-tests-build`, LLVM/clang
22.1.8, and cached ANTLR/Catch2 sources. Production compiler sources were not
changed while writing the tests.

| Check | Result |
| --- | --- |
| Build compiler, runtime, and test binaries | Passed |
| D3 semantic analysis of new program fixtures | 40/40 passed |
| Runtime-runner oracle checks | 11/11 passed |
| Manifest, traceability, and lifecycle consistency | Passed |
| Preexisting unit tests | 594/594 passed |
| Preexisting system tests | 244/244 passed |
| New D4 program runs, both optimization modes | 0/80 reached execution: compilation stops at the explicit D4 code-generation gate |
| New sanitized memory runs, both modes | 0/70 reached execution: the same compilation gate blocks them |
| New Catch2 IR cases | T45 passes; T41–T44 and T46–T48 fail while SOP lowering is absent |

T45 confirms the existing return-value/cleanup ordering mechanism. Its pass does
not establish that sequence allocation or recursive cleanup is implemented.
The new tests remain failing implementation targets; they are not skipped.

Reproduction commands, with `TOPC` and `TOPCLANG` selecting the isolated build:

```bash
export TOPC=/tmp/sopc-d4-tests-build/src/topc
export TOPCLANG=/usr/lib/llvm-22/bin/clang
bin/run-sop-tests.sh --semantic-only --jobs 4 --junit /tmp/sopc-d4-semantic.xml
bin/run-sop-tests.sh --jobs 4 --junit /tmp/sopc-d4-system.xml
bin/run-sop-memory-tests.sh --jobs 4 --junit /tmp/sopc-d4-memory.xml
ctest --test-dir /tmp/sopc-d4-tests-build -R '^SOP[ _]D4' --output-on-failure
ctest --test-dir /tmp/sopc-d4-tests-build -E '^SOP[ _]D4' --output-on-failure
python3 test/system/run.py --exclude-sop-d4 -j 4 --junit /tmp/sopc-d4-legacy-system.xml
```

The legacy system run passed outside the tool sandbox: LeakSanitizer cannot run
under the sandbox's tracing, which was also reproduced with an unchanged TOP
control program. The isolated build and reports are temporary session artifacts.

After implementing lowering, rerun the complete suite through `bin/runtests.sh`,
the dedicated memory runner, and coverage inspection through `bin/gencov.sh`.
Replace the planned walkthrough with actual compiler-emitted IR and update these
results. Sanitized SOP execution and D4 implementation coverage are not yet proven.
