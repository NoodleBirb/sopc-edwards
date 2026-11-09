# Deliverable 3 meeting demos

Run these commands from the repository root. The current meeting build is:

```bash
TOPC=/tmp/sopc-meeting-build/src/topc
```

SOP execution and LLVM lowering belong to Deliverable 4. These demonstrations
show inferred types and semantic plans. Use `--pownership` for the full semantic
check: `--ptype` alone does not validate moves and lifetimes.

## Main walkthrough

```bash
"$TOPC" --ptype programs/d3/ownership_and_borrows.sop
"$TOPC" --pownership --pborrow --constraint programs/d3/ownership_and_borrows.sop
```

Look for:

- `s` and `t`: `Seq(own&int)`; `e`: `borrow&own&int`; `g`: `(int) -> int`;
  the range binder `i`: `int`.
- Ownership moves from `p` into the sequence, and from `s` through `pass` to `t`.
- `main : 1 destroy`, targeting `t`. Other functions have zero named destroys.
- Recursive cleanup of the owning integer elements before sequence storage.
- Temporary `mk()` cleanup after its length is read; it adds no named destroy.
- `[borrow]` for `&t[0]` and `[loop-borrow]` for `&t`.

The generic scheme for `pass.q` remains a type variable and displays Copy in
the declaration view. At this owning call instantiation, its effect transfers
ownership. The move trace and cleanup plan show the call-site behavior.

## Sequence inference and independent slices

```bash
"$TOPC" --ptype --pownership programs/d3/sequences_and_slices.sop
```

Expect `nested : Seq(Seq(int))`, `source` and `slice : Seq(int)`, and
`first : int`. The empty inner literal is resolved by its sibling's type.
`main` has three named destroys: `nested`, `slice`, and `source`.

## Higher-order lambda and control-flow views

```bash
"$TOPC" --ptype --pownership programs/d3/higher_order_lambda.sop
mkdir -p /tmp/sopc-d3-demo-graphs
"$TOPC" --pcallgraph=ascii --constraint --output-dir /tmp/sopc-d3-demo-graphs programs/d3/higher_order_lambda.sop
cat /tmp/sopc-d3-demo-graphs/higher_order_lambda.sop.callgraph.txt
"$TOPC" --pcfg=ascii programs/d3/ownership_and_borrows.sop
```

Expect the indirect call `fn(argument)` to target the synthetic lambda. The
lambda's source expression and location appear in the type view. Function
values are Copy and `main` has zero named destroys. Each loop CFG has a body
edge, a continuation edge, and a back edge.

## Expected rejections

Each command below should exit nonzero with a semantic diagnostic:

```bash
"$TOPC" --pownership programs/d3/reject_use_after_move.sop
"$TOPC" --pownership programs/d3/reject_move_while_borrowed.sop
"$TOPC" --pownership programs/d3/reject_owned_slice.sop
"$TOPC" --pownership programs/d3/reject_unresolved_empty.sop
"$TOPC" --pownership programs/d3/reject_unknown_element_cleanup.sop
```

Expected diagnostic fragments, respectively:

1. `used after move`
2. `borrows it`
3. `slice cannot copy owned elements`
4. `sequence literal element type is undetermined`
5. `callee lacks element type information for recursive cleanup`

The older files directly under `programs/` are Deliverable 2 frontend examples;
some deliberately contain forms that Deliverable 3 semantic analysis rejects.
