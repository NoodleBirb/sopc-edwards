# Deliverable 1 change log

Current test policy (2026-09-17): only user-authored REQUIREMENTS1.md cases are
used for the new SOP suite. Earlier assistant-added tests described below were
removed in T003; their records and counts remain as history.

Assistant-authored record. REQUIREMENTS1.md remains student-authored.

## P001 — Preparation for user review (2026-09-15)

- Authorization: the user requested a reviewable plan before implementation and documentation of every change.
- Files added: IMPLEMENTATION_PLAN1.md and CHANGELOG1.md.
- Reason: make proposed grammar, tests, parser entry point, validation, and unresolved choices traceable before code changes.
- Evidence inspected: current requirements, pasted catalog excerpts, source grammar, parser tests/helpers, frontend pipeline, driver, and build documentation/configuration.
- Behavior change: none. No grammar, tests, compiler source, or requirements were modified.
- Validation: read-only repository inspection; no build or tests run in this planning step.
- Limitations: full PDF catalog not verified; implementation decisions D1–D5 are proposals pending user review. No implementation has begun.

## P002 — Approved plan and local build setup (2026-09-15)

- Authorization: user approved D1–D5. Updated IMPLEMENTATION_PLAN1.md to record that approval; the original proposals remain visible as decisions.
- Changed .gitignore to exclude /build-make/, the local Make build directory. The documented Ninja generator is unavailable; the initial build/ configure failed before compilation.
- Configure command: `cmake -S . -B build-make -G 'Unix Makefiles' -DLLVM_DIR=/usr/lib/llvm-22/lib/cmake/llvm -DCODE_COVERAGE=OFF -DCMAKE_C_COMPILER=/usr/lib/llvm-22/bin/clang -DCMAKE_CXX_COMPILER=/usr/lib/llvm-22/bin/clang++`.
- Dependency fetch initially failed on sandbox DNS. Re-ran with approved network access. Re-downloaded the empty ANTLR 4.13.1 jar left by the failed fetch. These are generated build artifacts, not source edits.
- No change to REQUIREMENTS1.md or dependency versions.

## T001 — Catalog tests (2026-09-15)

- Added test/unit/frontend/SOPParserTest.cpp with every accepted/rejected fragment from the three supplied catalog sections. Each fragment has an identifiable Catch2 dynamic section.
- Statements are wrapped in `main() { ... return 0; }` because the parser entry rule expects a program. Unresolved names remain deliberate to isolate syntax from name/type analysis.
- The new file is initially unregistered while the original baseline builds. Registration and red/green results will be recorded below.

## B001 — Baseline portability fix (2026-09-15)

- Original build failed in src/semantic/types/TypeInference.cpp: three calls to `std::find` lacked a matching declaration under Clang 22 with this standard library.
- Added the missing direct `<algorithm>` include. No algorithm or semantic behavior changed. This is a build prerequisite, separate from SOP changes.
- Initial command: `cmake --build build-make -j 6`; failure output captured in /tmp/sopc-baseline-build.log. Rebuilding after the include fix before establishing baseline results.

## G001 — SOP grammar and complete-input parsing (2026-09-15)

- Changed topg4/TOP.g4: added labeled alternatives for literals, indexing, slices, length, and lambdas; added loop/header rules and `for`/`by` tokens; required EOF at the program boundary.
- Sources: REQUIREMENTS1.md, supplied catalog, approved D1–D4. Slice bounds, range endpoints/step, lambda body, and loop binding are separately identifiable in the generated parse tree. A separate lambda rule allows a complete expression body without accidentally restricting it through outer operator precedence.
- Existing TOP expression labels and relative operator ordering are retained. New postfix forms bind above length, and length above arithmetic (D1). `by` is restricted to explicit ranges (D3); blocks are required by the student's draft.
- EOF rejects trailing garbage instead of accepting only a valid prefix. Name/type validity remains unchecked.
- Registered SOPParserTest.cpp in test/unit/frontend/CMakeLists.txt. Original parser run: six catalog test cases, three passed (rejections) and three failed (20 accepted fragments), captured in /tmp/sopc-catalog-red.log.
- Baseline after B001: all 478 CTest tests passed. System baseline in the sandbox: 50/154 passed; an unrestricted rerun was requested to investigate the compiled-program failures. Grammar verification pending below.

## P003 — Parser-only public entry point (2026-09-15)

- Changed FrontEnd.h/FrontEnd.cpp: `parseSyntax` validates a whole program using existing lexer/parser exception listeners and returns the printed ANTLR tree before parser-owned objects expire.
- Changed src/topc.cpp: `--parse-only` prints that tree and stops before AST/semantic/codegen work (D5). Incompatible analysis/compilation flags fail explicitly rather than being silently ignored.
- Added a frontend visitor guard on the normal AST path. Evidence: ASTBuilder inherits generated visitor defaults, which can visit SOP children without constructing corresponding nodes, risking silently incorrect TOP ASTs. The guard reports unsupported SOP AST construction and points to `--parse-only`; it does not misclassify valid SOP as a parse error. Added the matching driver diagnostic catch. This is a necessary D5 integration detail discovered during implementation.
- Updated README.md with command behavior, phase limits, keyword compatibility, and links to the plan/log. No SOP execution or typing was added.
- Verification pending syntax/API/CLI tests below.

## T002 — Boundary, structure, API, and CLI checks (2026-09-15)

- Extended SOPParserTest.cpp: tests mixed/chained expressions, omitted bounds, nested loops/lambdas, rejected malformed neighbors, and deferred semantic checks (unresolved names, duplicate lambda parameters, unsuitable operand types, and zero steps).
- Structure assertions check element/parameter order, D1 grouping, lambda body nesting, slice-bound omission, and loop binding/endpoints/step/body. Whole-input/API tests verify syntax exceptions and the explicit unsupported-AST diagnostic for every new construct.
- Added test/system/sop-parser-cli.cmake and registered it in test/CMakeLists.txt. It exercises the actual executable: parse tree and exit status, no bitcode output, syntactic/lexical/trailing-input errors, normal-path diagnostic, and conflicting command options. Artifacts are confined to the build directory.
- All 27 catalog assertions passed after the grammar changes. Baseline system rerun outside the sandbox passed all 154 tests; no original TOP test expectations were changed.
- Final build/test results are recorded below after these checks run.

## G002 — Preserve TOP program identity after EOF validation (2026-09-15)

- Full CTest run found 3 failures out of 494, all existing program-hash expectations. Cause: adding EOF made `ctx->getText()` append `<EOF>` to the input used for ASTProgram's SHA-256 name.
- Changed src/frontend/ast/ASTBuilder.cpp to concatenate program children except the EOF terminal when hashing. Source order and existing normalized token text are preserved.
- This fixes a regression rather than changing existing tests to accept it. Existing hash and traversal tests remain unchanged and directly validate compatibility.
- Focused SOP run before this fix: all 92 assertions across 15 cases passed; CLI integration also passed. Rebuilding and rerunning regressions after the hash fix.
- The concurrent system run also exposed one call-graph snapshot failure (153/154 passed). Initially suspected a shared hash cause; the follow-up run disproved that hypothesis (see R001). Its expected snapshot is unchanged.
- Final source review added explicit standard-library includes for `std::any`, `std::string`, and `std::pair`, formatted the new guard methods, and documented the AST API's unsupported-SOP exception in FrontEnd.h. These complete P003/T002 without changing syntax decisions.

## R001 — Deterministic existing call-graph output (2026-09-15)

- After G002 all 494 CTest tests passed, but the system suite remained at 153/154. Direct comparison showed identical call targets printed as `{square, addone}` rather than the existing snapshot's `{addone, square}`.
- Evidence: CallGraph.h stores targets as `std::set<ASTFunction *>`; src/topc.cpp printed this pointer order directly. The new parser's allocations exposed this existing output-order dependency, not a change in inferred call targets.
- Changed only printCallGraphConstraints in src/topc.cpp to sort target names before rendering. Analysis sets and existing expected snapshots are unchanged. The existing system snapshot directly exercises this fix.
- This follow-up supersedes the initial hash hypothesis for the system failure. Rebuild and final regression rerun follow.

## V001 — Final validation and file inventory (2026-09-15)

Results after all implementation changes:

- Build succeeded: `cmake --build build-make -j 6`; after the final driver-only change, `cmake --build build-make --target topc -j 6` also succeeded.
- Focused SOP tests: `build-make/test/unit/frontend/frontend_unit_tests '[SOP Parser]'` — 92 assertions in 15 cases passed. This includes all 27 supplied catalog fragments and the added boundary/API/structure checks.
- Full unit/integration suite: `ctest --test-dir build-make --output-on-failure -j 6` — 494/494 passed, including the actual CLI test and every original TOP test.
- System suite: `TOPC=/home/ethan/projects/sopc/build-make/src/topc TOPCLANG=/usr/lib/llvm-22/bin/clang RTLIB=/home/ethan/projects/sopc/build-make/rtlib python3 test/system/run.py -j 4` — 154/154 passed outside the sandbox, matching the environment used for the successful baseline.
- The existing call-graph snapshot also matched a direct `diff -u` against the final executable's output.
- `git diff --check` passed. No original TOP expected result or snapshot was changed.

Final changed-file inventory (all assistant edits):

| File | Purpose / decision record |
| --- | --- |
| IMPLEMENTATION_PLAN1.md | P001/P002: reviewable plan, approval of D1–D5, and completion status |
| CHANGELOG1.md | Decision history, failures, fixes, validation, and this inventory |
| .gitignore | P002: exclude local Make build artifacts |
| topg4/TOP.g4 | G001: three SOP features, labeled parse structure, and EOF |
| src/frontend/FrontEnd.h | P003: syntax-only API and phase-boundary documentation |
| src/frontend/FrontEnd.cpp | P003: syntax-only API and unsupported-AST guard |
| src/topc.cpp | P003: parser-only CLI and diagnostics; R001: stable call-target display |
| src/frontend/ast/ASTBuilder.cpp | G002: exclude EOF from existing program hashes |
| src/semantic/types/TypeInference.cpp | B001: missing standard-library include required for baseline build |
| test/unit/frontend/SOPParserTest.cpp | T001/T002: catalog, boundaries, structure, API, and semantic non-goals |
| test/unit/frontend/CMakeLists.txt | T001: register new parser test source |
| test/system/sop-parser-cli.cmake | T002: executable integration checks |
| test/CMakeLists.txt | T002: register CLI integration test |
| README.md | P003: usage, supported phases, and keyword compatibility |

REQUIREMENTS1.md remains unchanged by the assistant and retains its pre-existing untracked status. Generated parser/build files reside in ignored build directories; temporary investigation outputs reside under /tmp. No commit was created.

Scope and limits: the three supplied catalog sections are covered; unseen PDF sections remain unverified. SOP validation is available through `build-make/src/topc --parse-only program.top`. SOP AST construction, type checking, and execution remain later-phase work under approved D5. `for` and `by` are reserved under approved D4.

## R002 — System runner runtime-library default (2026-09-15)

- User reported 106 failures from `python3 test/system/run.py -j 2`. The pasted output identifies missing `rtlib/top_rtlib.bc` at link time; both build/rtlib/top_rtlib.bc and build-make/rtlib/top_rtlib.bc exist.
- Changed test/system/run.py (additional file beyond V001's inventory): default RTLIB to the runtime directory in the selected TOPC build, rather than the source directory. An explicit RTLIB environment override still takes precedence.
- Reason: CMake emits the runtime into the build tree. Earlier successful validation supplied RTLIB explicitly and therefore did not expose this incorrect default. The new default also follows an alternate TOPC path.
- Updated the runner's environment documentation to describe the new default. Validation pending rerun without RTLIB below.
- Validation: `env -u RTLIB TOPCLANG=/usr/lib/llvm-22/bin/clang python3 test/system/run.py -j 2` passed all 154 tests outside the sandbox. Direct configuration checks also passed for the default build, an alternate TOPC build, and an explicit RTLIB override. `git diff --check` passed.

## D001 — Complete test documentation (2026-09-15)

- User requested documentation for every test written or changed.
- Added TEST_DOCUMENTATION1.md: exact names of all 15 added Catch2 cases, each input/boundary and expected outcome, every CLI integration check, sources of expectations, test-helper behavior, infrastructure changes, and commands to run tests.
- Distinguished committed tests from the three ad hoc runner-configuration checks. Explicitly recorded that no original test assertion or snapshot changed.
- Documentation only; no test or implementation behavior changed. Checked the inventory against source and its assertion counts; no redundant test rerun required.

## F001 — SOP source filenames and manual validation (2026-09-15)

- User asked how to validate a file and whether the repository should support `.sop`.
- Inspection confirmed the compiler already reads arbitrary input filenames, so no lexer/parser extension change is needed. Existing `.top` regression inputs remain intact.
- README.md now demonstrates a complete `.sop` program, `--parse-only`, and exit-status checking; explains that extensions do not select different grammars or enable SOP execution.
- Changed test/system/sop-parser-cli.cmake fixtures to `.sop` and updated TEST_DOCUMENTATION1.md accordingly. Existing assertions are unchanged; this specifically verifies the newly documented extension.
- Changed bin/build.sh to recognize `.sop` alongside `.top` and strip either extension for executable names. Added `--parse-only` to modes that skip linking so stale bitcode cannot be linked after a syntax-only invocation. This helper still requires its existing compiler/runtime environment; direct `topc --parse-only` is the recommended syntax check.
- Validation recorded below.
- Validation: `bash -n bin/build.sh` passed; `ctest --test-dir build -R '^sop_parser_cli$' --output-on-failure` passed with `.sop` fixtures. Ad hoc checks compiled and linked a TOP-compatible program with each extension, confirmed executable naming, and verified `--parse-only` skipped invalid stale bitcode. Those checks used temporary files and are documented in TEST_DOCUMENTATION1.md. `git diff --check` passed.

## T003 — Replace assistant-selected tests with the user's cases (2026-09-17)

- User explicitly requested removal of all assistant-added tests and replacement with their updated REQUIREMENTS1.md Test Cases section. Further test suggestions require their agreement before addition.
- Replaced all 15 prior cases in test/unit/frontend/SOPParserTest.cpp with six cases containing exactly the user's 29 fragments. The sole correction is an added semicolon on accepted `x4 = s1[1+1:3]`, explicitly approved by the user for the fixture only.
- Deleted test/system/sop-parser-cli.cmake and removed its registration from test/CMakeLists.txt. Removed all prior assistant-selected structural, API, and additional boundary assertions. Original TOP tests and the runner's runtime-location fix remain unchanged.
- Updated topg4/TOP.g4 to allow an optional single `by` clause on iterable input, required by the user's accepted `for (i : s by y) { x = i; }`. This supersedes D3; it does not change the existing single-step restriction.
- Replaced TEST_DOCUMENTATION1.md with an exact inventory of the current cases and commands. Documented that the rejected block-lambda fragment also lacks a semicolon, so it does not isolate the block-body restriction. No unapproved additional case was added.
- Added a revision notice to IMPLEMENTATION_PLAN1.md and this log so superseded decisions/counts are not mistaken for the current suite.
- REQUIREMENTS1.md and the user's changes to programs/forloops.sop and programs/sequences.sop were left untouched. The earlier request for four additional example files was superseded before those files were created.
- Validation results follow below.
- Validation: build succeeded; `[requirements]` passed all 29 assertions in six cases; full CTest passed 484/484; existing system suite passed 154/154 outside the sandbox. An inventory comparison confirmed the fixtures exactly match all user cases except the approved semicolon correction.
- Whitespace review: `git diff --check` reported only a pre-existing user edit (trailing whitespace in REQUIREMENTS1.md), which was left untouched. The assistant's changed files pass the check.
