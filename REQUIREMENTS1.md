The requirements of Deliverable #1 are simple; improve the parsing tree to account for new SOP syntax. It's important to note that the execution will not be required for this step, only a successful parse. As a result of that constraint, anything that involves a specific typed expression will be ignored and still parse.

## 1. Sequences
These are similar to lists, and may contain expressions as defined in the grammar, and even other sequences.
A possible configuration of a sequence may be:
`s = [1, 2*2, 3];`
Each value of a sequence is separated by a comma, and there must always be an expression of some kind on either side of the comma. Sequences without a comma may contain either one or zero expressions:
`s = []; s = [1];`
Sequences may also contain sequences inside of them.
Sequences also have specific syntax usages outside of creation. A sequence should be able to be indexed by the following syntax:
`x = s[a]; x = s[a + b];`
To account for the possibility of a sequence in a sequence, multiple index calls can be chained together as follows:
`x = s[0][1];`
The length of a sequence should also be accessible with the following prefix operation:
`x = #s;`
Slices, or subsequences, should also be allowed:
`x = s[a:b]; x = s[a:]; x = s[:b]; x = s[:]; x = s[a + b:c];`
However, slices should only take up to two operands, with the omission meaning it extends all the way in the omitted direction. Adding more colons does not add more operands and should be treated as a parse error, such as:
`x = s[1:2:3];`

When something passes the parse check, it Accepts and builds a parse structure, otherwise it Rejects and creates a parse error.

## 2. Ranges and Iteration
Currently, TOP only implements while loops, so in order to implement for loops, we need ranges and the syntax to iterate on those ranges.

The main structure of a for loop is as follows:

`for (i : range by x) { x = i; }`

`for` is the keyword that denotes that an iteration is going to begin. The first argument inside the parentheses of an iterator must always be a single variable label. Afterwards must be a colon followed by what the range is going to be. After the range, there is an optional "by" keyword that may be allowed, which must then be followed by a single expression. Following the parentheses, or loop declaration, it must have curly braces denoting the function body. Assuming the function body is parseable, following these rules, the parser Accepts and builds a parse structure.

If there are multiple `by` keywords in a single loop header, it Rejects and creates a parse error.

Ranges are either defined with the following format:

`x .. y`

Where `x` and `y` are expressions.

In a for loop, an expression may also be accepted as a range. For example, some expression `s` or some borrow of an expression `&s` Accepts and builds a parse structure.

Ranges cannot be defined in the `x .. y` format outside of a `for` definition; it Rejects, for example, `x = 1 .. 5;`, and creates a parse error.

## 3. Lambdas
Lambdas are expressions defined by the following base structure:

`f = (x) => x + 1;`

A lambda is declared with parentheses, which will be referred to as the argument declaration, with at least one variable, with multiple variables separated by a comma, such as:

`f = (x, y) => x + y;`

If the parentheses are empty, it Rejects and throws a parse error. Following the parentheses must be a `=>` (lambda arrow) symbol. Following that symbol must be an expression.

If following the lambda arrow is not an expression, it Rejects and throws a parse error. For example,

`f = (x) => { return x; };`

is not allowed.