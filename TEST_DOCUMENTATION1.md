# Deliverable 1 test documentation

Assistant-authored test inventory. This documents implemented tests; it does not replace the student's requirements. There are **15 added Catch2 test cases (92 assertions)** and **one added CTest CLI integration test**. No existing test assertion or expected snapshot was changed.

Sources of expectations: the catalog pasted in the conversation, REQUIREMENTS1.md, and approved decisions D1–D5 in [IMPLEMENTATION_PLAN1.md](IMPLEMENTATION_PLAN1.md). Additional malformed cases generalize those rules; they are not additional quoted catalog requirements. Changes and validation history are in [CHANGELOG1.md](CHANGELOG1.md).

## How the parser tests work

All 15 cases are in [SOPParserTest.cpp](test/unit/frontend/SOPParserTest.cpp). Each numbered heading below gives its exact Catch2/CTest name. Every case has the `[SOP Parser]` tag; catalog, structure, and API cases additionally have the corresponding lowercase `catalog`, `structure`, or uppercase `API` tag.

The `program` helper wraps statement fragments in `main() { ... return 0; }`, since the grammar starts with a whole program. `accepts` checks that ParserHelper reports no lexer/parser errors; `rejects` checks that it reports an error. Dynamic sections identify individual fragments in failure reports. Unresolved names are intentional: these are syntax tests.

Structure cases call `FrontEnd::parseSyntax` and check specific fragments of the printed tree, not the entire tree's formatting. API error cases assert exception types. These tests do not evaluate programs, validate types or bounds, or establish SOP AST/code-generation support.

## 1. SOP catalog: sequence forms accept

13 assertions; direct catalog acceptance coverage.

| Fragment | What acceptance demonstrates |
| --- | --- |
| `s = [1, 2*2, 3];` | Ordered, comma-separated expression elements |
| `s = [];` | Empty literal |
| `s = [a, f(b), x + 1];` | Names, calls, and arithmetic as elements |
| `t = [[1, 42], [3]];` | Nested literals |
| `x = s[0];` | Indexing |
| `x = s[i + 1];` | Expression index |
| `x = #s;` | Prefix length |
| `x = #(s[1:3]);` | Length of a parenthesized slice |
| `x = s[lo:hi];` | Both slice bounds |
| `x = s[i+1:];` | Expression lower bound, omitted upper bound |
| `x = s[:#s];` | Omitted lower bound, length expression as upper bound |
| `x = s[:];` | Both bounds omitted |
| `x = s[0][1];` | Chained indexing |

## 2. SOP catalog: sequence boundaries reject

3 assertions; direct catalog rejection coverage.

| Fragment | Rejection boundary |
| --- | --- |
| `s = [1, 2, ];` | A comma requires another element |
| `x = s[1:2:3];` | Only one colon in a slice |
| `x = s#;` | Length is prefix, not postfix |

## 3. SOP catalog: iteration forms accept

4 assertions; direct catalog acceptance coverage.

| Fragment | What acceptance demonstrates |
| --- | --- |
| `for (i : min() .. hi) { x = i; }` | Expression endpoints; step omitted |
| `for (i : lo .. hi by 2*f()) { x = i; }` | Expression step |
| `for (e : s) { x = e; }` | Iterable expression instead of explicit range |
| `for (e : &s) { x = e; }` | Borrow expression as iterable input |

## 4. SOP catalog: range boundaries reject

2 assertions; direct catalog rejection coverage.

| Fragment | Rejection boundary |
| --- | --- |
| `x = 1 .. 5;` | Range notation is restricted to loop headers |
| `for (i : 1 .. 5 by 2 by 3) { x = i; }` | At most one step clause per header |

## 5. SOP catalog: lambda forms accept

3 assertions; direct catalog acceptance coverage.

| Fragment | What acceptance demonstrates |
| --- | --- |
| `f = (x) => x + 1;` | Single parameter and expression body |
| `g = (x, y) => f(x) + y;` | Multiple parameters; calls within body |
| `h = (x) => (y) => x + y;` | Lambda as another lambda's body |

## 6. SOP catalog: lambda boundaries reject

2 assertions; direct catalog rejection coverage.

| Fragment | Rejection boundary |
| --- | --- |
| `f = () => 5;` | Parameter list must be nonempty |
| `f = (x) => { return x; };` | Body must be an expression, not a block |

## 7. SOP parser: generalized valid syntax and deferred checks

18 acceptance assertions. These generalize the draft/catalog and approved decisions; semantic non-goals explain acceptance of unsuitable values.

| Fragment | Reason for the test |
| --- | --- |
| `s = [1];` | Singleton literal |
| `s = [[], [(x) => x], f(1)[0]];` | Composition of literals, lambdas, calls, and indexing |
| `x = f()[i][:][j];` | Mixed postfix chaining |
| `x = #s[1:3];` | Unparenthesized length/slice combination under D1 |
| `x = ##s;` | Nested prefix expressions |
| `x = 42[0];` | Index receiver type is not checked by parsing |
| `x = #42;` | Length operand type is not checked |
| `x = s[missing():f()];` | Expression bounds; no name resolution |
| `x = s[(a) => a];` | Index can be syntactically an expression even if its type is unsuitable |
| `x = ((x) => x + 1)(2);` | Calling a parenthesized lambda expression |
| `f = (x) => (y) => (z) => x + y * z;` | Three nested lambda bodies |
| `f = (x, x) => missing;` | Duplicate parameters and unresolved names are deferred checks |
| `for (e : 42) {}` | Iterable type is deferred |
| `for (e : &missing) {}` | Borrow/name validity is deferred |
| `for (i : f(1) + 2 .. g()[0] by 0) {}` | Complex endpoints; zero step is not rejected syntactically |
| `for (i : 5 .. 1 by -1) {}` | Descending endpoints and negative literal step |
| `for (i : 1 .. 5 by 2) { for (j : 1 .. 3 by 1) {} }` | Step-clause restriction is local to each header |
| `for (i : 1 .. 5) {} for (j : s) {}` | Consecutive loops of different forms |

## 8. SOP parser: neighboring malformed forms reject

28 rejection assertions. These test nearby invalid strings, not runtime errors.

| Fragment | Reason for rejection |
| --- | --- |
| `s = [,1];` | Missing first element |
| `s = [1,,2];` | Missing element between commas |
| `s = [1 2];` | Missing separator |
| `s = [1;` | Missing closing bracket |
| `x = s[];` | Index expression missing; unlike a slice, no colon is present |
| `x = s[::];` | Extra slice colon |
| `x = s[1,2];` | Comma-separated indices unsupported |
| `x = #;` | Missing length operand |
| `for (i + 1 : s) {}` | Binding must be a single identifier |
| `for (i : .. hi) {}` | Missing lower range endpoint |
| `for (i : lo ..) {}` | Missing upper range endpoint |
| `for (i : lo .. hi by) {}` | Missing step expression |
| `for (i : lo .. hi by 2 3) {}` | Extra expression without an operator/separator |
| `for (i : lo .. hi .. end) {}` | Extra range delimiter |
| `for (e : s by 2) {}` | D3 permits steps only for explicit ranges |
| `for (e : &s by 2) {}` | Same D3 restriction on borrowed iterable input |
| `for (e : s) x = e;` | Student requirements require a block body |
| `for (e : s) { return e; }` | Existing TOP restriction forbids nested return statements |
| `x = [1 .. 5];` | Standalone range expression unsupported inside a literal |
| `x = f(1 .. 5);` | Standalone range expression unsupported as an argument |
| `f = (x,) => x;` | Trailing parameter comma (D2) |
| `f = (,x) => x;` | Missing first parameter |
| `f = (1) => 1;` | Parameter must be an identifier (D2) |
| `f = (x + y) => x;` | Parameter cannot be an arbitrary expression (D2) |
| `f = (x) =>;` | Missing lambda body |
| `f = x => x;` | Missing parameter parentheses |
| `for = 1;` | Reserved keyword cannot be an identifier (D4) |
| `by = 1;` | Reserved keyword cannot be an identifier (D4) |

## 9. SOP parser: sequence tree preserves elements and postfix grouping

Input fragment: `s = [1, 2*2, []]; x = #s[0][1] + 2;`.

Two tree assertions verify:

1. Literal children retain their order: `1`, multiplication `2*2`, and empty literal.
2. The second expression groups as `(#((s[0])[1])) + 2`: postfix indexing nests on the left, binds inside length, and length binds inside addition (D1).

This checks relationships, not a computed result.

## 10. SOP parser: slice tree distinguishes omitted bounds

Four tree assertions, with each expression assigned to `x`:

| Expression | Required tree distinction |
| --- | --- |
| `s[:]` | Neither bound exists |
| `s[lo:]` | Only lower bound exists |
| `s[:hi]` | Only upper bound exists |
| `s[lo:hi]` | Both bounds exist in their respective positions |

No default bounds are evaluated or inserted by these assertions.

## 11. SOP parser: lambda tree preserves parameter order and nested body

One tree assertion for `h = (x, y) => (z) => x + y * z;`: outer parameters remain `x, y`; the outer body is a lambda with parameter `z`; its body groups as `x + (y * z)`. Sources: D2 and existing TOP multiplication precedence.

## 12. SOP parser: loop tree separates range step and body

One tree assertion for `for (i : lo .. hi by 2*f()) { x = i; }`: binding is `i`; the range contains `lo`, `hi`, and multiplication step `2*f()`; the block containing assignment `x = i` is outside the header. Source: catalog and the proposed tree roles approved in the plan.

## 13. SOP parser: syntax API rejects whole-input errors

Four assertions require `FrontEnd::parseSyntax` to throw `ParseError`. These inputs are whole sources and are **not** wrapped by the helper.

| Source | Boundary checked |
| --- | --- |
| Empty string | A program requires at least one declaration/function |
| `main() { return 0; } x = 1;` | A valid program prefix must not hide trailing invalid syntax |
| `main() { return 0; } @` | A trailing lexical error must be detected |
| `main() { x = s[1:2:3]; return 0; }` | Public syntax API propagates a parser error |

## 14. SOP parser: syntax API defers semantic checks

One no-exception assertion for `for (e : &missing) { f = (x, x) => #42; }`, wrapped as a program. This simultaneously checks that the public syntax API does not perform name resolution, parameter-uniqueness checking, or operand-type checking (D5).

## 15. SOP parser: normal AST path diagnoses unsupported constructs

Six assertions call **FrontEnd::parse**, not parseSyntax, and require `SemanticError` for the following wrapped fragments:

| Fragment | Unsupported AST construct |
| --- | --- |
| `s = [];` | Sequence literal |
| `x = s[0];` | Indexing |
| `x = s[:];` | Slicing |
| `x = #s;` | Length |
| `f = (x) => x;` | Lambda |
| `for (e : s) {}` | For loop |

Under D5 these constructs parse but do not yet have AST support. This test prevents the inherited visitor from silently creating incomplete ASTs. It checks exception type; the CLI test below checks the user-facing diagnostic.

## 16. sop_parser_cli

Source: [sop-parser-cli.cmake](test/system/sop-parser-cli.cmake). Registered as one CTest test, not a Catch2 case. It launches the actual compiler and creates input files in the build directory's `test/sop-parser-cli/` directory.

The fixture is saved as `sop.sop`; invalid inputs use `invalid.sop`. This exercises
`.sop` input filenames through the actual CLI. Existing TOP tests continue to use
`.top`, verifying compatibility. The filename extension does not choose a grammar.

Its valid whole-program fixture is:

```text
main() { s = [1, 2]; for (e : &s) { f = (x) => x + e; } return #s; }
```

Every check in the script:

| Invocation/input | Expected result and purpose |
| --- | --- |
| `--parse-only` on valid fixture | Exit 0; output contains `(program ` and `<EOF>`; validates public syntax route |
| After that valid invocation | Neither source-associated `.bc` nor `.ll` file exists; verifies no code generation |
| `--parse-only` on `main() { x = s[1:2:3]; return 0; }` | Exit 1, stderr contains `parse error`, stdout empty |
| `--parse-only` on `main() { return 0; } x = 1;` | Same error/empty-output contract for trailing syntax |
| `--parse-only` on `main() { return 0; } @` | Same contract for a lexical error |
| Normal compiler invocation on valid fixture | Exit 1; diagnostic includes `use --parse-only` and does not say `parse error`; distinguishes unsupported AST work from invalid syntax |
| `--parse-only -o unexpected.bc` on valid fixture | Exit 1; diagnostic says `cannot be combined`; requested output does not exist |
| `--parse-only --past` on valid fixture | Exit 1; diagnostic says `cannot be combined`; AST inspection is incompatible with stopping at parsing |

## Existing tests and test infrastructure

No existing test case or expected result was edited. In particular, existing program-hash/traversal assertions and the call-graph snapshot were preserved; implementation fixes restored their results (G002/R001 in the change log).

The following infrastructure files were changed:

| File | Change |
| --- | --- |
| `test/unit/frontend/CMakeLists.txt` | Adds SOPParserTest.cpp to the existing frontend test executable |
| `test/CMakeLists.txt` | Registers sop_parser_cli and supplies the built executable and work directory |
| `test/system/run.py` | Corrects default RTLIB to follow the selected TOPC build; updates environment documentation; leaves all system-test assertions intact |

For the runner fix, three **ad hoc configuration checks**, not committed automated test cases, verified the standard build path, an alternate TOPC path, and explicit RTLIB override precedence. The complete 154-test system suite then passed without an RTLIB override. These checks are recorded in R002 rather than counted in the 92 assertions.

For `.sop` filename support (F001), ad hoc build-helper checks compiled and linked
a TOP-compatible program from each of `.sop` and `.top`, verifying the executable
name has neither extension. A further check ran `--parse-only` on SOP input with
invalid stale bitcode present and verified no executable was linked. These are
not additional committed test cases. The existing CLI integration test passed
with its renamed `.sop` fixtures.

## Running these tests

Run from the repository root. Substitute `build-make` for `build` to use that build's binaries. Build first after source changes.

```bash
# List tests without executing them
ctest --test-dir build -N

# One exact-name test (CTest's -R argument is a regular expression)
ctest --test-dir build -R '^SOP catalog: lambda forms accept$' --output-on-failure

# All 15 SOP Catch2 cases through CTest
ctest --test-dir build -R '^SOP ' --output-on-failure

# Executable integration test
ctest --test-dir build -R '^sop_parser_cli$' --output-on-failure

# Catch2 tag selection, directly
build/test/unit/frontend/frontend_unit_tests '[SOP Parser][structure]'

# All registered tests
ctest --test-dir build --output-on-failure

# Existing system suite (requires TOPCLANG in the environment)
python3 test/system/run.py -j 2
```

The system runner currently has no individual-test filter. Its existing system tests and `sop_parser_cli` are separate suites. Last implementation validation: 494/494 CTest tests and 154/154 system tests passed; this documentation-only update does not change their behavior.
