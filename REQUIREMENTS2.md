The FrontEnd of SOP must be extended in order to include the additions of Sequences, Lambdas, and for loops. The structure of these requirements will differ, with each main section of the FrontEnd being separated, then separating by Sequence, Lambda, and for loops. Each requirement will also have a link to test cases that ensure this requirement exists.

# Parsing
R1. A previous parse pass shall have occurred and a resulting Parse Structure shall have been created.
Refer to REQUIREMENTS1.md Tests.

# AST Construction
R2. The AST shall be built following the Parse Structure.
Tests: T7
R3. All ASTs built shall guarantee that every child is reachable, and every child shall preserve the child-expression structure. Child order shall be deterministic, and shall have post-order visitor support.
Tests: T1, T2
R4. --past shall support the new AST constructs, with child nesting following the same patterns already built into TOP.
Tests: T3
R5. Each AST Node shall preserve the starting location as the starting line and column of the original source, starting at line 1 and column 0.
Tests: T4
R6. A parent AST Node shall preserve the children nodes while needed by the AST.
Tests: T5
R7. A parent AST Node shall allow access to the children nodes.
Tests: T6
R8. An AST Node shall be built as a class that follows the structures defined in the rest of the requirements.
Tests: T8, T9, T10, T11, T12, T13, T14

## Sequences
R9. Sequences shall fit the following structure:
```
Sequence:
  --elements:
    --expr
    --...
    --expr
```
The elements shall contain ordered expressions, or no expressions.
Tests: T8

R10. Slices shall fit the following structure:
```
Slice:
  --sequence: expr
  --lower_bound: expr | None
  --upper_bound: expr | None
```
The lower_bound and upper_bound shall have exist as either another expression, or be denoted as None, preserving the existence of that argument.
Tests: T9

R11. Indexing a Sequence shall have the following structure:
```
Index:
    --sequence: expr
    --index: expr
```
The index and sequence shall have an expression.
Tests: T10

R12. The length of a sequence shall have the following structure:
```
SeqLength:
  --sequence: expr
```
The sequence shall have an expression.
Tests: T11
## Lambdas
R13. Lambdas shall have the following structure:
```
Lambda:
  --parameters:
    --ParameterDeclaration()
    --...
    --ParameterDeclaration()
  --body: expr
```
Each Lambda shall have at least one parameter, and the body shall have an expression.
Tests: T12

## Ranges and Iteration
R14. Ranges shall have the following structure:
```
Range:
  --start: expr
  --end: expr
```
Each Range shall have both a start and an end expression. Ranges shall only appear as part of a ForLoop.
Tests: T13
R15. For loop iteration shall have the following structure:
```
ForLoop:
  --iterator: binder
  --range: Range | expr
  --by: expr | None
  --body:
    --statement
    --...
    --statement
```
Each ForLoop shall have an iterator as an binder to a name declaration. Each ForLoop shall have a range, as either a Range or an expression. Each ForLoop shall have a `by` as either an expr or None. Each ForLoop shall have a body of ordered statements. The body shall be allowed to be empty.
Tests: T14
# Traversal
R16. Traversal of a node shall move across the node's children, its elements, via the source order of a node. 
Tests: T2, T15
R17. If the traversal finds a child node missing, it shall skip it. 
Tests: T16, T17
R18. Traversal shall be done in post-order. The entirety of a child's subtree shall be traversed before the parent is visited.
Tests: T2, T18
# Pretty Printing
R19. Upon Pretty Printing the AST, parsing and creating an AST using the Pretty Printed AST shall result in the an AST of equivalent structure being formed, along with identical text being printed via the second AST.
Tests: T19
R20. Binary operators shall have one space on the left and right. T20
Tests:
R21. Binary expressions shall be surrounded by parentheses.
Tests: T20
R22. Expression children shall preserve their own printing rules.
Tests: T21
## Sequences
R23. The element list shall be enclosed in brackets.
Tests: T22
R24. If there are more than one expression in a sequence, each expression shall be separated by a comma and one space after the comma.
Tests:
R25. Slice expressions shall have no whitespace between the `:` and the expressions to its side.
Tests: T24
R26. The Slice expression's bounds and colon shall be surrounded by brackets, with the previous expression touching the opening bracket on its left.
Tests: T25, T26
R27. Empty Slice expression bounds shall have nothing printed.
Tests: T27
R28. The SeqLength expression shall remove parentheses around Slice expressions if present.
Tests: T28
R29. The SeqLength expression shall append a `#` to the front of the expression it contains.
Tests: T29
R30. The Index expression shall have first expression at the front, touching brackets enclosing the `index` expression.
Tests: T30
## Lambdas
R31. Lambda parameters shall be enclosed in parentheses, with each parameter separated by a comma and a single space after the comma.
Tests: T31
R32. Between the lambda parameters and the body, there shall be a `=>` with a space on both sides.
Tests: T32
## Ranges and Iteration
R33. The right side of the iterator in a ForLoop shall be a colon with a single space on each side.
Tests: T33
R34. Ranges shall have a `..` symbol with a space on both sides separating the start and end expressions.
Tests: T34
R35. ForLoops shall enclose the iterator through the `by` in parentheses. If the by is not present, instead enclose through the range.
Tests: T35
R36. Before the enclosing parentheses, the `for` keyword shall be written with a space in between.
Tests: T36
R37. If the `by` is present, it shall have a space on its left and right and the word `by` separating the range and `by` expression.
Tests: T37
R38. The ForLoop body shall be enclosed in curly braces on new lines, with the body statements containing two spaces before them, including any previous spaces from nesting. Each statement shall also contain a new line separating them.
Tests: T38

# Symbols and Scopes
R39. Duplicate name declarations within a Lambda parameter list shall result in a symbol error.
Tests: T39
R40. Lambda parameter declarations shall be scoped inside of the Lambda, and become unavailable outside of its scope.
Tests: T40
R41. The ForLoop binder shall have been an existing var belonging to the enclosing function, else it shall result in a symbol error.
Tests: T41
R42. The ForLoop shall not introduce new declarations or scope.
Tests: T41
R43. The --psym argument shall separate Lambdas from functions, and have its own output for lambda params. --psym shall have the following structure:
```
// Illustrated example of how a Lambda may be filled in.
Functions : {}
Lambdas: {(a, b) => (a + b) @3:8}
Locals for function main : {}
Params for lambda (a, b) => (a + b) @3:8 : {a, b} 
// omitted if there are no Lambdas
```
Each Lambda shall be entered into the Lambdas section as normalized source text followed by a space and the saved line and column in the format `@l:c`.
Tests: T42
# Weeding
R44. The following shall not be assignable: 
- SeqLength
- Slice
- Index
- Lambda
- Sequence
Attempts to assign to these shall result in an Assignment Error.
Tests: T43
R45. The following shall be Borrowable:
- Index
Attempts to borrow any other new constructs will result in an Address of Error.
Tests: T44

# Phase Boundaries and Compatibility
R46. Type-checking shall be pushed to later stages.
Tests: T45
R47. Ownership shall be pushed to later stages.
Tests: None
R48. Lowering shall be pushed to later stages.
Tests: None
R49. Code Generation shall be pushed to later stages.
Tests: None

# Tests
T1. Build an AST and traverse over it. Denote the size of the AST, and check if it traversed over the same number of Unique Nodes. Run twice to ensure deterministic behavior.
T2. Create a mock AST that takes `3 + 5`, and check that the visitor order follows `3, 5, +`, to follow post-order convention. 
T3. Create a sequence, ForLoop, slice a sequence, index a sequence, get the length of a sequence, fill the ForLoop with a range and by keyword, and create a Lambda. Ensure --past output contains all of them in the expected format.
T4. Create a mock program with a known starting line and column for a random declaration. Ensure the stored starting line and column match the source.
T5. Create a parent node. Move the node outside of the original space, removing the local pointer from the initial space, and ensure that access to the node's children works the same. Do this for every new construct.
T6. Create a node, and populate its children. Try accessing them. Do this for every new construct.
T7. Create a parse tree, and save an expeted AST result based on that parse tree. Create the AST and assert that they are equal.
T8. Attempt making a sequence that is empty, one element, or three elements. Compare the output to the expected AST output.
T9. Attempt slicing a sequence with empty, only lower_bound, only upper_bound, and both bounds, and compare to the expected AST.
T10. Attempt indexing a Sequence with an expression and show it successfully creates an AST with the expected structure.
T11. Attempt using the length expression on an expression and show it creates the expected structure.
T12. Attempt creating a Lambda with proper structure and show it matches an expected AST output.
T13. Attempt creating a Range with an expression on both sides, and show it matches an expected AST structure.
T14. Attempt creating a ForLoop with a Range, a ForLoop with an expr, a ForLoop with a by, and a ForLoop without a by. Show that all match an expected AST structure.
T15. Create an AST structure for all new constructs, and traverse starting from the source of the node. Ensure it visits every child in the source order.
T16. Create a Slice missing the lower_bound, missing the upper_bound, and missing both, and try traversing the AST structure. For every omitted section, don't visit it, and proceed as normal.
T17. Create a ForLoop with the `by`, and without the `by`. Try traversing the AST structures outputted, and show that without the `by`, it isn't visited and it proceeds as normal.
T18. Create a sequence with a lambda, another sequence, and a random expression nested inside. Traverse the resulting AST and ensure it follows post-order traversal correctly by matching it to an expected output.
T19. Parse a sample program and run it through the FrontEnd and Pretty Print the outputted AST. Run this printed text through the parser and FrontEnd again, and confirm the Pretty Print output is the same, and the AST structure is the same.
T20. Gather every binary operator in a small program, parse, construct an AST, and then pretty print it, and confirm that each usage is surrounded by parentheses and spaced correctly.
T21. Nest a binary operator, and a length operator formatted as `#(s[:])`, and show that both follow their own pretty printing rules inside of the sequence.
T22. Create a sequence, parse, and construct an AST. Show the pretty printed output containing brackets around the element list for Sequences with 0, 1, or 2 elements.
T23. Create a sequence with three elements, parse, and construct an AST. Show the pretty printed output containing commas separating each element with a space following.
T24. Create a slice expression with only lower_bound, only upper_bound, both, or none, parse, build an AST, and show the pretty printed output containing no whitespace within the brackets, outside of individual expressions' personal rules.
T25. Create a slice expression with only lower_bound, only upper_bound, both, or none, parse, build an AST, and show the pretty printed output containing brackets enclosing everything to the right of the first expression.
T26. Create a slice expression with its first expression as a slice expression, parse, build an AST, and show the pretty printed output having two sets of brackets side by side.
T27. Create a slice expression with no bounds, and show that it prints as `s[:]`, where s is the chosen name of the first expression.
T28. Create a SeqLength expression and place it onto a Slice expression with parentheses surrounding it, such as `#(s[:])`. Parse, build an AST, and show the pretty printed output as removing the redundant parentheses.
T29. Create a SeqLength expression, parse it, and create an AST. Show that the pretty printed output contains a `#` at the front of the expression.
T30. Create an Index expression, parse it, build an AST, and show the pretty printed output containing brackets enclosing the index expression.
T31. Create a Lambda with one parameter, and another with two parameters, parse it, and build an AST. Show that in the pretty printed output, each parameter is separated by a comma, and a single space after the comma.
T32. Create a Lambda, parse it, and build an AST. Show that in the pretty printed output, there is a `=>` between the parameters and the body, and that there is a single space on both sides.
T33. Create a ForLoop expression. Parse it, build an AST, and show that in the pretty printed output a colon is between the iterator and the range with a single space on both sides of the colon.
T34. Create a ForLoop expression, and populate it with a Range, parse it, and build an AST. Show that the pretty printed outputted Ranges start and end values are separated by `..` and a single space on both sides.
T35. Create a ForLoop expression with the by, and a ForLoop expression without the by. Parse it, build an AST, and show that the pretty printed output parentheses encloses through the by if present, and through the range if not present.
T36. Create a ForLoop expression. Parse it, build an AST, and show that the pretty printed output has the keyword `for` to the left of the parentheses, and with a space between it and the parentheses.
T37. Create a ForLoop expression with the by, parse it, and build an AST. Show that the pretty printed output has the `by` keyword with a space on both sides, separating the range on the left, and the `by` expression on the right.
T38. Create a ForLoop expression with the by, parse it, and build an AST. Show that the pretty printed output has the body enclosed in curly braces on new lines, and the body statements are indented by two spaces on top of whatever the current indentation level is.
T39. Create a Lambda with arguments `(a, a)`, and show that it causes a symbol-processing error.
T40. Create a mini SOP program that has a Lambda with the argument `(a)`, and show that an attempt to use `a` outside of the Lambda results in a symbol error.
T41. Do not declare a var `i`, then try creating a ForLoop with the binder as `i`, and show that it throws a symbol error.
T42. Create an example script that contains several Lambda functions. Show that the --psym argument matches the Illustrated Example in R43, with the Lambdas appearing under their own lambda instead of enclosing-function locals.
T43. Show that attempting to run an assignment expression as if SeqLength, Slice, Index, Lambda, and Sequence are Left-values will result in an Assignment Error.
T44. Show that attempting to Borrow an Index expression and using it as a Left-value in an assignment expression will succeed, but borrowing any of the other new constructs and attempting to use them in the same way will result in an Address Of Error.
T45. Try slicing a random expression, such as `(3 + 1)`, and show that it creates an AST correctly.