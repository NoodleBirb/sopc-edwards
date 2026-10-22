# Deliverable 3 tests

`SOPD3Cases.cpp` contains source fixtures, requirement IDs, inferred local
types/classes, named destruction targets, ownership transitions, diagnostics,
and call-target expectations. Each fixture is a separate Catch2 section.

`SOPD3SemanticTest.cpp` runs full semantic analysis, checks the retained results
and inserted destruction nodes, and checks loop CFG edges and lambda call
targets. Negative cases require `SemanticError` with the listed diagnostic
fragments. The generic SOP `not implemented` error always fails these cases.
Generic formal parameters are checked at their inferred level; a concrete call
does not require specialization of a function's polymorphic scheme.

`SOPD3ViewsTest.cpp` invokes the built compiler and inspects production output
from `--ptype`, `--pownership`, `--pborrow`, `--pcfg=ascii`, and
`--pcallgraph=ascii`, with `--constraint`. Each invocation uses a separate
temporary directory, checks its exit status, and removes its artifacts.

The cleanup tests require additional labeled cleanup/destruction plan detail
separate from the named-owner summary. They check target/type information,
recursive element strategies, ordering descriptions, Copy-element omission,
temporary expression locations and cleanup points, and transferred ownership.
The heading may contain either `cleanup` or `destruction` and either `plan`
or `detail`. Recipe checks use descriptive fragments rather than a complete
snapshot. These inspection checks do not establish runtime memory behavior;
execution and sanitizer checks belong to Deliverable 4.

New diagnostic fragments in the fixtures specify the intended rejection
reason. They may be adjusted to the implemented diagnostic wording while
preserving that reason. Neither a parse failure nor an unrelated semantic
failure satisfies the tests.

| Group | Coverage |
|---|---|
| G1 | SL1–SL2, SL15; literal element types and inference |
| G2 | SL4–SL5, SL11–SL13, SL17; construction and ownership states |
| G3 | SL3, SL6–SL8, SL14, SL16, SL18–SL19; retained and temporary cleanup |
| G4 | IX1–IX3, IX7; indexing types and owned extraction |
| G5 | IX4–IX6, IX8–IX9; indexed borrows and mutation; SL5 storage boundary |
| G6 | LN1–LN7; length and borrowed reads; unnamed cleanup also in G3 |
| G7 | SC1–SC7; independent slices and Copy restrictions; temporary cleanup in G3 |
| G8 | RG1–RG6; range typing and CFG; changing loop states also in G2 |
| G9 | BI1–BI2, BI5, BI7–BI8; iterable and binder types |
| G10 | BI3–BI4, BI6, BI9; protected loop lifetime and binder uses |
| G11 | LA1–LA4, LA7; lambda types, scope, arity, and possible targets |
| G12 | SL9–SL10, LA5–LA6, LA8; call effects and cleanup knowledge |

Build `semantic_unit_tests` with CMake. Run from the build directory:

```sh
test/unit/semantic/semantic_unit_tests '[D3]'
test/unit/semantic/semantic_unit_tests '[D3][fixtures]'
test/unit/semantic/semantic_unit_tests '[D3][G5]'
test/unit/semantic/semantic_unit_tests '[D3][cleanup-plan]'
test/unit/semantic/semantic_unit_tests '~[D3]'
```

The semantic and view tests exercise the implemented SOP analyses and cleanup
plan. They are registered normally, alongside the existing TOP regression
suite, and are neither skipped nor marked as expected failures.
