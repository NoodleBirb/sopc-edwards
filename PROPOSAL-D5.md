# Goal and Motivation
The goal is to introduce a new sequence generator function built into the language. One of my qualms with SOPC right now is the lack of dynamic sequences, and this attempts to reconcile that. Without dynamic sequences, I see little point in even using sequences when they're more complicated than multiple variables, other than for simple looping. With dynamic sequences, the program could actually accomplish something using its data, and save many lines of code.

## What Changes
A new keyword, `generate` will be added. This will have to be routed through the parsing -> code generation pipeline to ensure it acquires functionality throughout the language. 
The following will now be accepted:
```
f(x) {
    return x;
}

main() {
    s = generate(4, f);
    return s[0];
}
```
and
```
s = generate(3, (i) => i + 1);
```
The following will be rejected:
```
s = generate(3, (i) => [i]);
```
and
```
s = generate(4, (i) => alloc i);
```

## Claim
This extension adds a generator that, at runtime, creates a sequence of n elements of class Copy following a function f.

## Compiler Phases Affected
Parser - New symbol
Frontend - New AST Structure
Type Analysis - Ensures types match, and that ownership is applied to created sequence correctly
Code Generation - Implement the run-time generation of a sequence.

## Test Plan
T1: Test that `generate(4, f);` parses.
T2: Ensure that the AST structure is formed correctly; will be outlined in D5 requirements assuming this passes the proposal check.
T3: Show that function returns of type Copy succeed.
T4: Show that something that isn't a function/lambda will result in a type error compile.
T5: Show that generate cannot be used as an L-Value.