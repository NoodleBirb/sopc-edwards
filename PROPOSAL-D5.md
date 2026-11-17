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