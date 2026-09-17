# Deliverable 1 test documentation — user-authored cases

Updated 2026-09-17. The current SOP suite contains only the cases in the student's
REQUIREMENTS1.md **Test Cases** section: six Catch2 cases, with 29 individual
fragments. Every earlier assistant-selected SOP test was removed, including the
CLI integration test, structural assertions, API tests, and additional boundaries.
Existing TOP regression tests are unchanged. Additional tests require explicit
user approval before they are added.

Source: [SOPParserTest.cpp](test/unit/frontend/SOPParserTest.cpp).
Each fragment gets its own dynamic section, wrapped in
`main() { FRAGMENT return 0; }` because the parser accepts whole programs.
Acceptance uses `ParserHelper::is_parsable`; rejection asserts its negation.
No name resolution, type checking, execution, or parse-tree-shape assertions are
performed by these cases. All six cases have `[SOP Parser][requirements]` tags.

## Approved fixture correction

The accepted `x4 = s1[1+1:3]` line lacks a semicolon in REQUIREMENTS1.md.
The user explicitly approved adding it in the fixture only. All other fragments
are copied verbatim; REQUIREMENTS1.md was not edited.

The rejected `f = (x) => { return x; }` fragment also lacks the assignment's final
semicolon. It is preserved as written: its rejection alone does not isolate the
block-body restriction from the missing-terminator error. No additional test was
added to isolate those conditions.

## Current cases

### SOP requirements: sequences accept

10 assertions; every fragment must parse successfully.

```text
s1 = [1, 2, 3];
s2 = [];
s3 = [5*3];
s4 = [[1, 2], [1, 3]];
x1 = #s1;
x2 = s1[0];
x3 = s1[1:];
x4 = s1[1+1:3];
x5 = s3[:];
x6 = s4[0][:1];
```

### SOP requirements: sequences reject

4 assertions; every fragment must produce a lexer/parser error.

```text
s1 = [1, ];
s2 = [,];
x1 = s1#;
x2 = s1[0:1:2];
```

### SOP requirements: iteration accepts

6 assertions; every fragment must parse successfully.

```text
for (i : 0 .. 2) { x = i; }
for (i : s) { x = i; }
for (i : s by y) { x = i; }
for (i : &s) { x = i; }
for (i : min() .. max() by 5 - 3) { x = i; }
for (i : 2 + 3 .. 5 + 6 by -1) { x = i; }
```

### SOP requirements: iteration rejects

3 assertions; every fragment must produce a lexer/parser error.

```text
for (i) { x = i; }
for (i : 0 .. 2 by 1 by 1) { x = i; }
x = 1 .. 4;
```

### SOP requirements: lambdas accept

3 assertions; every fragment must parse successfully.

```text
f = (x) => x + 1;
g = (x, y) => f(x) + y;
h = (x) => (y) => x + y;
```

### SOP requirements: lambdas reject

3 assertions; every fragment must produce a lexer/parser error.

```text
f = () => 5;
f = (x) => { return x; }
f = (x, ) => x + 1;
```

## Removed tests and infrastructure

- Replaced all 15 previous assistant-selected cases (92 assertions) in SOPParserTest.cpp.
- Deleted test/system/sop-parser-cli.cmake and its CTest registration in test/CMakeLists.txt.
- Kept the SOPParserTest.cpp build registration so the six replacement cases run normally.
- Kept the test/system/run.py runtime-location fix; this is runner configuration, not an added test.
- No existing TOP assertions or snapshots were edited or removed.

The user's accepted `for (i : s by y) { x = i; }` supersedes the old D3 restriction.
The grammar now permits a single step clause for iterable inputs as well as
explicit ranges. This is an implementation change required by the user's test,
not an additional test decision.

## Running the current cases

```bash
cmake --build build --target frontend_unit_tests
ctest --test-dir build -R '^SOP requirements:' --output-on-failure
ctest --test-dir build -R '^SOP requirements: sequences accept$' --output-on-failure
build/test/unit/frontend/frontend_unit_tests '[requirements]'
```

Use `build-make` instead of `build` if using that build tree, and rebuild it first.
The deleted `sop_parser_cli` test and the old catalog/structure/API case names are
no longer available after CMake regenerates that build's test registration.
Validation results are recorded in CHANGELOG1.md; earlier counts there are
historical and refer to the superseded suite.
