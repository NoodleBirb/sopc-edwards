The FrontEnd of SOP must be extended in order to include the additions of Sequences, Lambdas, and for loops. The structure of these requirements will differ, with each main section of the FrontEnd being separated, then separating by Sequence, Lambda, and for loops. Each requirement will also have a link to test cases that ensure this requirement exists.

# Parsing
R1. A previous parse pass shall have occurred and a resulting Parse Structure shall have been created.
Refer to REQUIREMENTS1.md Tests.

# AST Construction
R2. All ASTs built shall guarantee that every child is reachable, and every child shall preserve the child-expression structure. Child order shall be deterministic, and shall guarantee have post-order visitor support.
Tests: 
R3. --past shall support the new AST constructs, with child nesting following the same patterns already built into TOP.
Tests:
R4. Each AST Node shall preserve the starting location as the starting line and column of the original source, starting at line 1 and column 0.
Tests:
R5. A parent AST Node shall preserve the children nodes while needed by the AST.
Tests:
R6. A parent AST Node shall allow access to the children nodes.
Tests:
R7. An AST Node shall be built as a class that follows the structures defined in the rest of the requirements.
Tests:
R8. The AST shall be built following the Parse Structure.

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

R10. Slices shall fit the following structure:
```
Slice:
  --sequence: expr
  --lower_bound: expr | None
  --upper_bound: expr | None
```
The lower_bound and upper_bound shall have exist as either another expression, or be denoted as None, preserving the existence of that argument.
Tests:

R11. Indexing a Sequence shall have the following structure:
```
Index:
    --sequence: expr
    --index: expr
```
The index and sequence shall have an expression.
Tests:

R12. The length of a sequence shall have the following structure:
```
SeqLength:
  --sequence: expr
```
The sequence shall have an expression.
Tests:
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
Tests:

## Ranges and Iteration
R14. Ranges shall have the following structure:
```
Range:
  --start: expr
  --end: expr
```
Each Range shall have both a start and an end expression. Ranges shall only appear as part of a ForLoop.
Tests:
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
Tests:
# Traversal
R16. Traversal of a node shall move across the node's children, its elements, via the source order of a node. 
Tests:
R17. If the traversal finds a child node missing, it shall skip it. 
Tests:
R18. Traversal shall be done in post-order. The entirety of a child's subtree shall be traversed before the parent is visited.
Tests:
# Pretty Printing
R19. Upon Pretty Printing the AST, parsing and creating an AST using the Pretty Printed AST shall result in the an AST of equivalent structure being formed, along with identical text being printed via the second AST.
Tests:
R20. Binary operators shall have one space on the left and right.
Tests:
R21. Binary expressions shall be surrounded by parentheses.
Tests:
R22. Expression children shall preserve their own printing rules.
## Sequences
R23. The element list shall be enclosed in brackets.
Tests:
R24. If there are more than one expression in a sequence, each expression shall be separated by a comma and one space after the comma.
Tests:
R25. Slice expressions shall have no whitespace between the `:` and the expressions to its side.
Tests:
R26. The Slice expression's bounds and colon shall be surrounded by brackets, with the previous expression touching the opening bracket on its left.
Tests:
R27. Empty Slice expression bounds shall have nothing printed.
Tests:
R28. The SeqLength expression shall remove parentheses around Slice expressions if present.
Tests:
R29. The SeqLength expression shall append a `#` to the front of the expression it contains.
Tests:
R30. The Index expression shall have the sequence at the front, touching brackets enclosing the `index` expression.
## Lambdas
R31. Lambda parameters shall be enclosed in parentheses, with each parameter separated by a comma and a single space after the comma.
Tests:
R32. Between the lambda parameters and the body, there shall be a `=>` with a space on both sides.

## Ranges and Iteration
R33. The right side of the iterator in a ForLoop shall be a colon with a single space on each side.
Tests:
R34. Ranges shall have a `..` symbol with a space on both sides separating the start and end expressions.
Tests:
R35. ForLoops shall enclose the iterator through the `by` in parentheses. If the by is not present, instead enclose through the range.
Tests:
R36. Before the enclosing parentheses, the `for` keyword shall be written with a space in between.
Tests:
R37. If the `by` is present, it shall have a space on its left and right and the word `by` separating the range and `by` expression.
Tests:
R38. The ForLoop body shall be enclosed in curly braces on new lines, with the body statements containing two spaces before them, including any previous spaces from nesting. Each statement shall also contain a new line separating them.
Tests:

# Symbols and Scopes
R39. Duplicate name declarations within a Lambda parameter list shall result in a symbol error.
Tests:
R40. Lambda parameter declarations shall be scoped inside of the Lambda, and become unavailable outside of its scope.
Tests:
R41. The ForLoop binder shall have been an existing var belonging to the enclosing function, else it shall result in a symbol error.
R42. The ForLoop shall not introduce new declarations or scope.
Tests:
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

# Weeding
R44. The following shall not be assignable: 
- SeqLength
- Slice
- Index
- Lambda
- Sequence
Attempts to assign to these shall result in an Assignment Error.
Tests:
R45. The following shall be Borrowable:
- Index
Attempts to borrow any other new constructs will result in an Address of Error.
Tests:

# Phase Boundaries and Compatibility
R46. Type-checking shall be pushed to later stages.
Tests:
R47. Ownership shall be pushed to later stages.
Tests:
R48. Lowering shall be pushed to later stages.
Tests:
R49. Code Generation shall be pushed to later stages.
Tests:

# Tests
T1. Get parse structure from some random SOP program.
T2. Create an