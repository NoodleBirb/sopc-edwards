The FrontEnd of SOP must be extended in order to include the additions of Sequences, Lambdas, and for loops. The structure of these requirements will differ, with each main section of the FrontEnd being separated, then separating by Sequence, Lambda, and for loops. Each requirement will also have a link to test cases that ensure this requirement exists.

# Parsing

R1. A previous parse pass shall have occurred and a resulting Parse Structure shall have been created.
Refer to REQUIREMENTS1.md Tests.

# AST Construction

R2. All ASTs built shall guarantee that every child is reachable, and every child shall preserve the child-expression structure. Child order shall be deterministic, and shall guarantee have post-order visitor support.
Tests: 
R3. --past shall support the new AST constructs, with child nesting following the same patterns already built into TOP.
Tests:
## Sequences
R3. Sequences shall fit the following structure:
```
Sequence:
  --elements:
    --expr
    --...
    --expr
```
The elements shall contain ordered expressions, or no expressions.

R4. Slices shall fit the following structure:
```
Slice:
  --sequence: expr
  --lower_bound: expr | None
  --upper_bound: expr | None
```
The lower_bound and upper_bound shall have exist as either another expression, or be denoted as None, preserving the existence of that argument.
Tests:

R5. Indexing a Sequence shall have the following structure:
```
Index:
    --sequence: expr
    --index: expr
```
The index and sequence shall have an expression.
Tests:

R6. The length of a sequence shall have the following structure:
```
SeqLength:
  --sequence: expr
```
The sequence shall have an expression.
Tests:
## Lambdas
R7. Lambdas shall have the following structure:
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
R8. Ranges shall have the following structure:
```
Range:
  --start: expr
  --end: expr
```
Each Range shall have both a start and an end expression. Ranges shall only appear as part of a ForLoop.
Tests:
R9. For loop iteration shall have the following structure:
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
R10. Traversal of a node shall move across the node's children, its elements, via the source order of a node. 
Tests:
R11. If the traversal finds a child node missing, it shall skip it. 
Tests:
R12. Traversal shall be done in post-order. The entirety of a child's subtree shall be traversed before the parent is visited.
Tests:
# Pretty Printing
R13. Upon Pretty Printing the AST, parsing and creating an AST using the Pretty Printed AST shall result in the an AST of equivalent structure being formed, along with identical text being printed via the second AST.
Tests:
R14. Binary operators shall have one space on the left and right.
Tests:
R15. Binary expressions shall be surrounded by parentheses.
Tests:
R16. Expression children shall preserve their own printing rules.
## Sequences
R17. The element list shall be enclosed in brackets.
Tests:
R18. If there are more than one expression in a sequence, each expression shall be separated by a comma and one space after the comma.
Tests:
R19. Slice expressions shall have no whitespace between the `:` and the expressions to its side.
Tests:
R20. The Slice expression's bounds and colon shall be surrounded by brackets, with the previous expression touching the opening bracket on its left.
Tests:
R21. Empty Slice expression bounds shall have nothing printed.
Tests:
R22. The SeqLength expression shall remove parentheses around Slice expressions if present.
Tests:
R23. The SeqLength expression shall append a `#` to the front of the expression it contains.
Tests:
R24. The Index expression shall have the sequence at the front, touching brackets enclosing the `index` expression.
## Lambdas
R25. Lambda parameters shall be enclosed in parentheses, with each parameter separated by a comma and a single space after the comma.
Tests:
R26. Between the lambda parameters and the body, there shall be a `=>` with a space on both sides.

## Ranges and Iteration
R27. The right side of the iterator in a ForLoop shall be a colon with a single space on each side.
Tests:
R28. Ranges shall have a `..` symbol with a space on both sides separating the start and end expressions.
Tests:
R29. ForLoops shall enclose the iterator through the `by` in parentheses. If the by is not present, instead enclose through the range.
Tests:
R30. Before the enclosing parentheses, the `for` keyword shall be written with a space in between.
Tests:
R31. If the `by` is present, it shall have a space on its left and right and the word `by` separating the range and `by` expression.
Tests:
R32. The ForLoop body shall be enclosed in curly braces on new lines, with the body statements containing two spaces before them, including any previous spaces from nesting. Each statement shall also contain a new line separating them.
Tests:

# Symbols and Scopes
R33. Duplicate name declarations within a Lambda parameter list shall result in a symbol error.
Tests:
R34. Lambda parameter declarations shall be scoped inside of the Lambda, and become unavailable outside of its scope.
Tests:
R35. The ForLoop binder shall have been an existing var belonging to the enclosing function, else it shall result in a symbol error.
R36. The ForLoop shall not introduce new declarations or scope.
Tests:
R37. The --psym argument shall separate Lambdas from functions, and have its own output for lambda params. --psym shall have the following structure:
```
// Illustrated example of how a Lambda may be filled in.
Functions : {}
Lambdas: {(a, b) => (a + b) @3:8}
Locals for function main : {}
Params for lambda (a, b) => (a + b) @3:8 : {a, b} 
// omitted if there are no Lambdas
```
Each Lambda shall be entered into the Lambdas section as normalized source text followed by a space and `@r:c` where `r` is the row and `c` is the column in which the Lambda was created in the original source text.

# Weeding
R38. The following shall not be assignable: 
- SeqLength
- Slice
- Index
- Lambda
- Sequence
Attempts to assign to these shall result in an Assignment Error.
Tests:
R39. The following shall be Borrowable:
- Index
Attempts to borrow any other new constructs will result in an Address of Error.
Tests:
