# Deliverable 4: Code Generation and Runtime Design

D4 lowers semantically validated SOP programs to verified LLVM IR. The runtime
requirements, tests, traceability, and memory-safety matrix are in
[REQUIREMENTS4.md](REQUIREMENTS4.md). New construct lowering lives in
[src/codegen/SOPCodeGen.cpp](src/codegen/SOPCodeGen.cpp); the existing visitor
registers it and preserves TOP's function-call and ownership conventions.

## Sequence Representation

A length-`n` sequence occupies one heap allocation of `n + 1` 64-bit words:

| Byte offset | Contents |
| --- | --- |
| `0` | Length `n` |
| `8 * (i + 1)` | Element at zero-based index `i` |

The sequence value is the allocation address represented as `i64`, following
TOP's existing convention. `inttoptr` recovers a pointer for access; `ptrtoint`
produces a source value. An empty sequence still allocates an eight-byte header.
Literals evaluate elements once, left to right, and allocate fresh storage.

For `[[1, 2], [3]]`:

```text
local slot for s: P

P: [ length 2 | Q | R ]                         24 bytes
                |   |
                |   +----> R: [ length 1 | 3 ]  16 bytes
                +--------> Q: [ length 2 | 1 | 2 ]  24 bytes
```

Each row has its own allocation and length. Outer elements store row addresses,
so ragged rows need no special representation. Static types distinguish integer,
function-index, and owned-address words.

## Bounds, Slices, and Range Continuation

Indexing evaluates the source and index once, in that order. It checks signed
`0 <= i < length` before computing and using the element slot. A value read loads
the word; a borrow returns the slot address without taking ownership. Source and
index operands are generated in value mode even when the requested result is an
address.

Failed checks call `_top_sop_error(category, line)` and end their LLVM block in
`unreachable`. The C runtime prints `OutOfBounds` or `ZeroStep` and exits nonzero.
No invalid element access or later source operation executes.

Slices evaluate the source, supplied lower bound, and supplied upper bound once,
left to right. Defaults are `0` and source length. After checking
`0 <= lo <= hi <= length`, lowering allocates `hi - lo + 1` words and copies
exactly `(hi - lo) * 8` element bytes with `llvm.memcpy`. Empty slices copy zero
bytes. Result storage is independent of the source. D3 permits only Copy
elements, so slicing requires no owned-element cloning. Work is O(k) and added
sequence storage is O(k + 1).

A range saves its start, end, and step before entering the loop. The default
step is `+1`; zero fails before binder assignment, even for an empty range. A
private SSA counter and a header `phi` implement iteration:

```text
counter = saved start
while (saved step > 0 ? counter < saved end : counter > saved end):
    binder = counter
    execute body
    (next, overflow) = signed_add_with_overflow(counter, saved step)
    if overflow: finish loop
    counter = next
continue after loop
```

Body changes to the binder or source bound variables cannot alter progression.
Only a successful header test assigns the binder. Empty loops preserve its
previous value. Overflow exits the loop and continues the program without using
a wrapped counter or assigning the binder again.

Borrowed sequence loops evaluate the iterable once, load its sequence address
through the borrowed slot, and visit increasing indices. Copy elements load
values into the binder. Own elements supply an `i64` address of the element
slot, typed `borrow&T` by D3. Loop completion acquires no cleanup responsibility
for the borrowed sequence or its elements.

The selected D3 binder convention deliberately differs from the handout's
additional explicit borrow when forwarding an owned-element binder. For example,
`sumrow(row)` forwards an already-borrowed row; `sumrow(&row)` adds another
reference level. T25 and T27 exercise the selected convention. D4 does not change
D3's typing or borrow rules.

## Owned Elements and Unnamed Sequences

Code generation follows D3's destruction plan. Named retained owners use its
inserted destruction statements. Moves and owning returns pass the existing
address and destruction responsibility to their recipient.

A cached helper per inferred sequence type frees the container. If elements are
Own, its generated loop first destroys each element recursively, then frees the
container. Existing sum helpers and sequence helpers can call one another.
Declarations enter the cache before their bodies are built, and helper generation
saves and restores the caller's insertion point.

For unnamed sequences, the visitor records the generated value and emits cleanup
at the plan's designated operation:

| Use | Release point |
| --- | --- |
| Read length | After the header load |
| Read a Copy element | After the element load |
| Supply a slice source | After the copy |
| Borrow a temporary element for a call or loop | After the receiving operation |
| Transfer into a binding, argument, sequence, or return | Recipient retains responsibility |

A return first computes its complete value, then destroys retained owners, then
emits `ret`. LLVM retains that computed SSA value across cleanup calls. T52 also
checks a nested temporary owner kept alive throughout borrowed iteration and
repeated temporary cleanup inside a range.

## Lambda Values and Generic Calls

Lambda preparation materializes each noncapturing body as an ordinary synthetic
function. Lowering emits that function and represents the lambda value as its
`i64` index in the function table. Evaluation creates no heap environment and
does not execute the body. Calls evaluate the function expression and arguments
once, left to right, then load and invoke the table entry. Ordinary ownership
contracts apply to arguments and results.

A generic borrowed loop may be instantiated with both Copy and Own elements.
Their binder representations differ, despite both fitting in `i64`. For programs
containing an unresolved iteration relation, lowering lazily emits function
variants using the validated argument/result types and D3's iteration relation.
Bindings propagate through named forwarding calls and materialized lambdas;
variants are cached and registered in the same function table. This preserves
the existing call ABI and avoids any special treatment of user-defined function
names. T51 checks one generic fold through both element modes, a forwarding
function, and a lambda relay.

## Actual IR Walkthrough

[walkthrough.top](docs/d4/walkthrough.top) contains a nontrivial sequence/range
program. Its captured, unoptimized output is
[walkthrough.ir](docs/d4/walkthrough.ir). It returns `80` for input `1`: the loop
sums `60`, then adds `s[1]`. Input `3` reports `OutOfBounds` on source line 8 and
exits with status 255.

| Emitted block | Source operation and emitted instructions |
| --- | --- |
| `_top_main:entry` | Read `index`; allocate the literal with `calloc(4, 8)`; store length `3` and elements `10,20,30`; initialize `total`; load `#s` once for the saved range end. |
| `sop.ok` | Enter the range after the step check. The default step is the constant `1`, so its branch condition is already `true`. |
| `sop.error` | Zero-step failure block, unreachable for this default-step program; calls the runtime and ends in `unreachable`. |
| `for.header` | `phi` selects initial `0` or `%range.next`; compare the counter to the saved length; choose body or exit. The default-positive-step selector is constant `true`. |
| `for.body` | Assign the counter to `i`; evaluate `total + s[i]` left to right; check the index against the current sequence length. |
| `sop.ok7` | Compute `(i + 1)` word offset; load the element; update `total`; use `llvm.sadd.with.overflow.i64` to advance; branch to the header or exit on overflow. |
| `sop.error8` | Body indexing failure: call category `1` on source line 6, then `unreachable`. |
| `for.exit` | Load `total`; evaluate final `s[index]`; check the command-line index before its element load. |
| `sop.ok15` | Load that element; compute `%add20`; call `_top_destroy_sequence.0` for `s`; return the already-computed `%add20`. |
| `sop.error16` | Final indexing failure: call category `1` on source line 8, then `unreachable`. |
| `_top_destroy_sequence.0:entry` | Convert the retained sequence address to `ptr`, call `free`, and return. Its elements are Copy integers. |

For example, the final success block contains this actual instruction ordering:

```llvm
%element.value19 = load i64, ptr %element.slot18, align 4
%add20 = add i64 %total9, %element.value19
%s.val = load i64, ptr %s, align 4
call void @_top_destroy_sequence.0(i64 %s.val)
call void @llvm.donothing()
ret i64 %add20
```

Reproduce and independently verify the listing with LLVM 22:

```bash
build/src/topc -do --asm docs/d4/walkthrough.top -o /tmp/d4-walkthrough.ll
/usr/lib/llvm-22/bin/opt -passes=verify -disable-output /tmp/d4-walkthrough.ll
```

## Validation and TOP Compatibility

Validation uses LLVM/clang 22.1.8. The suite has 42 program fixtures: 37 successful
programs and five failure programs with 23 parameterized inputs. Both optimization
modes are exercised. Runtime-failure oracles distinguish compile/link failure,
signals, diagnostic categories, and exact pre-failure output. Memory checks
instrument all successful programs and enable leak detection.

Results recorded on 2026-11-12:

| Check | Result |
| --- | --- |
| Workspace compiler, runtime, and test build | Passed |
| Complete CTest suite | 604/604 passed: 594 preexisting cases, eight D4 IR cases, and two test-infrastructure cases |
| Complete system suite through `bin/runtests.sh` | 364/364 passed: 244 preexisting checks and 120 D4 checks |
| D4 memory suite, both modes, ASan and leak detection | 74/74 passed; no sanitizer reports |
| Runtime-oracle unit checks | 11/11 passed, included by the CTest infrastructure case |
| Requirement/test manifest and six lifecycle rows | Consistent |
| Captured walkthrough IR | LLVM verifier passed; input `1` returns `80`; input `3` reports `OutOfBounds` and exits 255 |
| New lowering file line coverage | 96.7% (295/305 lines); all 15 functions exercised |

The unchanged TOP snapshots, driver checks, analysis cases, and executable system
programs establish regression evidence alongside the preexisting unit tests.
Both optimized and unoptimized SOP execution passes, including explicit runtime
failures and indirect calls with ownership transfer.

The complete workspace run used its coverage-enabled Debug build. The dedicated
memory run used the same implementation in an isolated Debug build with coverage
disabled. Sanitized executions ran outside the tool sandbox because LeakSanitizer
cannot inspect programs under its tracing. `bin/gencov.sh` generated
`coverage.info` and `coverage.out/`; overall captured line coverage was 95.8%.
Uncovered new-file lines include defensive paths and compiler-attributed closing
braces. Coverage does not replace the behavioral and sanitizer checks.

Reproduction from the repository root after building with LLVM 22:

```bash
export TOPCLANG=/usr/lib/llvm-22/bin/clang
bin/runtests.sh -- -j 1 --junit /tmp/sopc-d4-system.xml
bin/run-sop-tests.sh --check
bin/run-sop-memory-tests.sh --jobs 1 --junit /tmp/sopc-d4-memory.xml
bin/gencov.sh
```

`TOPC` and `RTLIB` can select an isolated build for the dedicated D4 runners.
The session's machine-readable reports are `/tmp/sopc-d4-workspace-units.xml`,
`/tmp/sopc-d4-workspace-system.xml`, and `/tmp/sopc-d4-memory-final.xml`.
