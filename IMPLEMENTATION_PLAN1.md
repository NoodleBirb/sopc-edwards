# Deliverable 1 implementation plan — approved D1–D5

This is an assistant-authored implementation plan, not a replacement for the student's REQUIREMENTS1.md. The user approved D1–D5 on 2026-09-15. Implementation proceeds for the three supplied catalog sections; completeness against any unseen sections remains unverified.

Completion: the approved three-feature scope is implemented and validated. All 494 CTest tests and 154 system tests pass. See CHANGELOG1.md for actual changes, intermediate failures, fixes, and commands. The parser-only option prints the parse tree; normal SOP compilation reports the phase limitation.

## Evidence and scope

Sources: the current REQUIREMENTS1.md and the sequence, range/iteration, and lambda catalog excerpts pasted into the conversation. The full PDF has not been read successfully; coverage of any additional catalog features is unverified.

The target is syntax acceptance/rejection and ANTLR parse structure. Type checking, name resolution, bounds checking, range execution, closure behavior, and code generation for SOP are outside this deliverable. Existing TOP compilation must continue working.

Repository findings:

- topg4/TOP.g4 contains the source grammar; CMake generates the parser and visitor. Generated files should not be edited manually.
- test/unit/frontend/TOPParserTest.cpp already checks both acceptance and printed parse-tree grouping through ParserHelper.
- FrontEnd::parse immediately builds an AST; src/topc.cpp then invokes later compiler phases. Grammar support alone does not make the normal compiler path support new SOP constructs.
- The program rule does not require EOF. Full-input validation needs an explicit check so a valid prefix cannot conceal rejected trailing input.
- No applicable AGENTS.md was found. REQUIREMENTS1.md is currently untracked and remains student-authored.

## Proposed work, in order

1. Establish a baseline. Configure/build using the existing CMake workflow and run current tests before editing implementation. Record dependency failures separately from code failures. LLVM 22 is present under /usr/lib/llvm-22; there is no workspace build directory yet. Dependency fetching may need network permission.
2. Add catalog tests in a new test/unit/frontend/SOPParserTest.cpp and register it in test/unit/frontend/CMakeLists.txt. Embed fragments in valid TOP functions with a final return. Copy every supplied accepted and rejected fragment into identifiable cases. Record expected failures against the original parser.
3. Extend topg4/TOP.g4 incrementally: sequences first, loops second, lambdas third. Preserve existing alternatives and labels wherever possible to avoid disturbing ASTBuilder and TOP precedence.
4. Add a syntax-only entry point in FrontEnd.h/FrontEnd.cpp and a proposed --parse-only option in src/topc.cpp. It should validate the complete input and optionally expose the printed parse tree, then stop before AST construction. Test valid input and lexer/parser errors through this path. The existing TOP compilation path remains available; this plan does not claim SOP AST or runtime support.
5. Add structural and boundary tests tied to the decisions below. Require complete input consumption, using EOF in the program rule if baseline tests show no legitimate prefix-parsing dependency. Check existing TOP acceptance and compiler behavior after that change.
6. Run focused SOP/parser tests, the full CTest suite, and the existing system harness. Record commands and results in CHANGELOG1.md. Any changed TOP expectation must cite the conflicting SOP rule; no existing expectation is currently identified as needing reversal.

## Feature mapping

| Feature | Planned grammar change | Test evidence |
| --- | --- | --- |
| Sequence literals | An optional comma-separated expression list inside brackets, with no trailing comma | Empty, expression-valued, nested literals; reject `[1, 2, ]` |
| Indexing and slicing | Distinct postfix forms; indexing requires an expression, slicing requires one colon with independently optional bounds | Every supplied indexing/slicing example, including chaining; reject a second slice colon |
| Length | Prefix `#` with an expression operand | `#s`, `#(s[1:3])`, `s[:#s]`; reject `s#` |
| Range iteration | A loop header with a binding identifier, endpoints, and an optional single step clause | Both explicit-range examples; reject repeated `by` and standalone `1 .. 5` |
| Sequence iteration | A loop header with a binding identifier and iterable expression | Both `s` and `&s`, without consulting symbol/type information |
| Loop body | Require a block, following the current student requirements | Accepted catalog blocks and a boundary test without braces |
| Lambdas | A nonempty identifier parameter list, `=>`, and expression body | Single/multiple parameters and nesting; reject empty parameters and a block body |

Structural checks will inspect ordered elements/parameters, operands and omitted slice bounds, loop binding/input/step/body, and lambda nesting. These are parser checks, not evaluation tests.

## Decisions approved by the user

These choices extend beyond what the pasted examples completely specify. They are approved implementation decisions, not catalog facts.

- D1: Calls, indexing, and slicing bind more tightly than prefix length; prefix length binds more tightly than arithmetic. Thus `#s[1:3]` groups as `#(s[1:3])`. Preserve the old relative precedence of TOP operators.
- D2: A lambda body is a complete expression, including another lambda. `(x) => (y) => x + y` has the second lambda as the first lambda's body. Reject trailing parameter commas and non-identifier parameters, consistent with the draft's parameter description.
- D3: Permit `by` only on explicit `lo .. hi` ranges. The examples do not show `for (e : s by 2)`, and the draft's use of “range” leaves this ambiguous.
- D4: Reserve `for` and `by` as keywords, consistent with TOP's other keyword tokens. This can make old uses of those words as identifiers invalid; record any actual affected tests/programs rather than hiding the compatibility consequence.
- D5: Use a parser-only CLI route for this deliverable instead of adding SOP nodes throughout AST, semantic analysis, and code generation. This provides a usable entry point without requiring later-deliverable work.

The user approved D1–D5. The braces requirement is already stated explicitly in the current REQUIREMENTS1.md. Scope remains the three supplied sections.

## Traceability during implementation

Each logical change gets a CHANGELOG1.md entry with an ID, affected files, supporting requirement/catalog case or reviewed decision, behavior before/after, tests and actual results, and any remaining limitation. Revisions to this plan are logged too. A final file inventory will account for every changed file. No student requirements will be edited by the assistant.
