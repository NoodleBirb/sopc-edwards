## Goal and Motivation
The goal is to introduce a new sequence generator function built into the language. One of my qualms with SOPC right now is the lack of dynamic sequences, and this attempts to reconcile that. Without dynamic sequences, I see little point in even using sequences when they're more complicated than multiple variables, other than for simple looping. With dynamic sequences, the program could actually accomplish something using its data, and save many lines of code. Using a generator such as this could allow us to implement a similar, yet different function: `map`. This will allow us to easily apply a function over a borrowed sequence, allowing for even more usability for more complex code interactions.

## What Changes
New keywords `generate` and `map` will be added. This will have to be routed through the parsing -> code generation pipeline to ensure it acquires functionality throughout the language. 
The following will now be accepted:
```
s = generate(4, f);
s2 = map(&s, (i) => i+1);
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
s = generate(3, (i) => i + 1);
s = map(s, (i) => i + 2);
```

## Claim
This extension adds a generator that, at runtime, creates a sequence of n elements of class Copy following a function f, and a map function that, at runtime, creates a new sequence s of #s elements of class Copy based on a borrowed sequence and a function f.

## Compiler Phases Affected
Parsing, Frontend, Type/Ownership Analysis, and Code Generation.

## Test Plan
T1: Test that `generate(4, f);` and `map(&s, (i) => i*i)` parses.
T2: Test that the construction of the AST for generate and map is of the expected output; to be defined in requirements.
T3: Show that attempting to use incorrect arguments or missing arguments does not compile. Incorrectly typed arguments fail type checking, and missing arguments fails parsing.
T4: Show that a sequence generated of <= 0 length does not compile.
T5: Show that at runtime, a sequence literal and a generation which should create the same literal are equivalent.
T6: Show that something that isn't a function/lambda will result in a type error.
T7: Show that generate/map cannot be used as an L-Value.

## Risks and Non-Goals
A risk with this plan is ensuring memory safety when the length of an allocated sequence is unknown until runtime. This is necessary in order to make the functionality work, but it still stands as a roadblock to figure out.
Something that is *not* a goal is to allow for Own class values to be generated/mapped. 

## Size
The assumed size of this project is similar to that of D3/D4. This means I expect around 1200+ lines of code written as an uncertain estimate.