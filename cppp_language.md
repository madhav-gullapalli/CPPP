# CP++ Language Reference

This document describes the implemented CP++ language surface as it exists today, based on `errors.txt` and `correct.txt`.

CP++ is a competitive-programming-oriented language that transpiles to C++. It focuses on making algorithm-heavy code shorter to write, easier to read, and easier to debug than raw contest-style C++, while still lowering to simple generated C++.

Complexities below describe the CP++ operation itself at the language level. Generated C++ details may vary a little, but the intent is the standard competitive-programming cost model.

## Compilation Modes

### Source files

- Syntax: `program.cppp`
- What it does: CP++ source files are transpiled to adjacent generated `.cpp` files.
- Notes: The compiler can also compile and run the generated C++.
- Complexity: N/A

### Transpile

- Syntax: `build/cppp --cppp file.cppp`
- What it does: Generates `file.cpp`.
- Notes: Good for inspecting lowered C++.
- Complexity: proportional to source size

### Run mode

- Syntax: `build/cppp --cppp file.cppp --run`
- What it does: Transpiles, compiles, and runs with extra CP++ runtime checks.
- Notes: This mode is where readable runtime diagnostics are preserved for list bounds, empty operations, division by zero, and similar checked behavior.
- Complexity: proportional to source size plus downstream C++ compile/runtime cost

### Submit mode

- Syntax: `build/cppp --cppp file.cppp --submit`
- What it does: Emits slimmer contest-oriented generated C++ and prunes unused helpers.
- Notes: Some extra runtime checking present in `--run` is intentionally not carried into submit-oriented output.
- Complexity: proportional to source size

## General Syntax

### Statements end with semicolons

- Syntax: `int a = 1;`
- What it does: Most ordinary statements are semicolon-terminated.
- Notes: Block headers such as `if (...) {` and `for (...) {` use braces and do not end with semicolons.
- Complexity: O(1)

### Multiple statements per line

- Syntax: `int a = 1; int b = 2; print(a + b);`
- What it does: CP++ accepts multiple semicolon-separated statements on one line.
- Notes: Useful for short contest code.
- Complexity: O(1)

### Braced blocks

- Syntax:
  ```cpp
  if (ok) {
      print("yes");
  }
  ```
- What it does: CP++ uses braces for block structure.
- Notes: Supported on `if`, `else if`, `else`, `while`, classic `for`, foreach `for (T x in list)`, and `rep(...)`.
- Complexity: O(1)

## Types

### `bool`

- Syntax: `bool ok = true;`
- What it does: Boolean type with literals `true` and `false`.
- Notes: Can participate in arithmetic-style numeric conversions supported by CP++.
- Complexity: O(1)

### `char`

- Syntax: `char c = 'A';`
- What it does: Single-character value.
- Notes: Must be exactly one character in single quotes.
- Complexity: O(1)

### `int`

- Syntax: `int x = 42;`
- What it does: Default integer type.
- Notes: Lowers to C++ `long long`, so this is a 64-bit integer type, not a 32-bit `int`.
- Complexity: O(1)

### `float`

- Syntax: `float x = 3.14;`
- What it does: Floating-point numeric type.
- Notes: Lowers to C++ `long double`.
- Complexity: O(1)

### `string`

- Syntax: `string s = "hello";`
- What it does: Built-in text type.
- Notes: Lowers to `List<char>`-style behavior with string-aware printing. Supports indexing, slicing, `add(...)`, `len(...)`, `in`, and `find(...)`.
- Complexity: O(1) for the type itself; operations vary

### `List<T>`

- Syntax: `List<int> values;`
- What it does: Generic dynamic list type.
- Notes: `List` expects exactly one subtype. Nested forms like `List<List<int>>` are supported.
- Complexity: O(1) for the type itself; operations vary

### Nested list types

- Syntax: `List<List<int>> grid = [[1, 2], [3]];`
- What it does: Supports lists of lists and deeper nesting.
- Notes: Recursive list literals and recursive list operations are part of the implemented surface.
- Complexity: O(1) for the type itself; operations vary

### Default values

- Syntax:
  ```cpp
  int x;
  bool ok;
  List<int> values;
  ```
- What it does: Declares a variable using its type's default value.
- Notes: The exact default value behavior is defined by the implementation and demonstrated in the executable docs.
- Complexity: O(1)

## Declarations and Assignment

### Single declaration

- Syntax: `int x = 3;`
- What it does: Declares a variable with an optional initializer.
- Notes: Variables cannot be redeclared in the same scope.
- Complexity: O(1)

### Multiple declaration assignment

- Syntax: `int a, b = 0, 1;`
- What it does: Declares multiple variables of the same type and initializes them positionally.
- Notes: Counts must match.
- Complexity: O(k) for `k` variables

### Assignment

- Syntax: `x = 5;`
- What it does: Replaces a variable's current value.
- Notes: Types must be assignable under CP++ conversion rules.
- Complexity: O(1) for scalars, otherwise depends on value size

### Multiple assignment

- Syntax: `x, y = y, x;`
- What it does: Assigns multiple targets from multiple values.
- Notes: Counts must match unless the right-hand side is target-typed `input()`.
- Complexity: O(k) for `k` targets

### Compound assignment

- Syntax: `x += 1;`
- What it does: Updates a value in place using a binary operator.
- Notes: Supported operators include `+=`, `-=`, `*=`, `/=`, `%=`, `<<=`, `>>=`, `&=`, `|=`, `^=`, `&&=`, and `||=`.
- Complexity: typically O(1), except for sequence concatenation-style cases

## Literals

### Boolean literals

- Syntax: `true`, `false`
- What it does: Produces `bool` values.
- Notes: Built in.
- Complexity: O(1)

### Character literals

- Syntax: `'x'`
- What it does: Produces a `char`.
- Notes: Must be exactly one character.
- Complexity: O(1)

### Integer literals

- Syntax: `123`
- What it does: Produces an `int`.
- Notes: Parsed as the CP++ integer type.
- Complexity: O(1)

### Floating literals

- Syntax: `2.5`
- What it does: Produces a `float`.
- Notes: Standard decimal syntax.
- Complexity: O(1)

### String literals

- Syntax: `"hello"`
- What it does: Produces a `string`.
- Notes: Printed as ordinary text.
- Complexity: O(n) in literal length to materialize

### List literals

- Syntax: `[1, 2, 3]`
- What it does: Produces a list literal.
- Notes: Element type is inferred from context such as `List<int> values = [1, 2, 3];`. Empty `[]` requires contextual type information.
- Complexity: O(n)

### Nested list literals

- Syntax: `[[1], [2, 3]]`
- What it does: Produces nested list values.
- Notes: Works recursively when the target type supports it.
- Complexity: O(total elements)

## Conversions and Casts

### Implicit numeric conversions

- Syntax:
  ```cpp
  int x = 5;
  float y = x;
  ```
- What it does: Allows the implemented implicit conversions across numeric-like scalar types.
- Notes: Supported conversions include `bool -> char/int/float`, `char -> bool/int/float`, `int -> bool/float`, and `float -> bool`.
- Complexity: O(1)

### Explicit casts

- Syntax: `(char)x`
- What it does: Converts a value explicitly.
- Notes: Use when an implicit conversion is rejected.
- Complexity: O(1)

## Expressions and Operators

### Variables

- Syntax: `x`
- What it does: Reads a declared value.
- Notes: Must already be in scope.
- Complexity: O(1)

### Parenthesized expressions

- Syntax: `(a + b)`
- What it does: Groups expression evaluation.
- Notes: Standard precedence override.
- Complexity: O(1) overhead

### Arithmetic operators

- Syntax: `+`, `-`, `*`, `/`, `%`
- What it does: Standard arithmetic.
- Notes: Integer `/` and `%` have extra runtime checking in `--run` for divide-by-zero and related edge cases.
- Complexity: O(1)

### Comparison operators

- Syntax: `<`, `<=`, `>`, `>=`, `==`, `!=`
- What it does: Compares values and returns `bool`.
- Notes: Lists of the same CP++ type are lexicographically comparable, including nested lists.
- Complexity: O(1) for scalars, up to O(n) for lists/strings in the compared prefix length

### Logical operators

- Syntax: `!`, `&&`, `||`
- What it does: Boolean logic.
- Notes: Conditions use bool expressions or values CP++ can convert to bool.
- Complexity: O(1)

### Bitwise operators

- Syntax: `&`, `|`, `^`, `<<`, `>>`
- What it does: Bitwise integer-style operations.
- Notes: Follows the implemented CP++ type rules.
- Complexity: O(1)

### Prefix and postfix increment/decrement

- Syntax: `++x`, `x++`, `--x`, `x--`
- What it does: Mutates mutable numeric-like variables in place.
- Notes: Supported for mutable `char`, `int`, and `float` values in the current implementation.
- Complexity: O(1)

### Absolute value

- Syntax: `abs(x)`
- What it does: Returns the absolute value of a numeric expression.
- Notes: Accepts numeric values.
- Complexity: O(1)

## Lists

### Append one element

- Syntax: `values.add(x);`
- What it does: Appends one value to the end of a list.
- Notes: The appended value must be compatible with the list element type.
- Complexity: amortized O(1)

### Insert one element at an index

- Syntax: `values.add(x, index);`
- What it does: Inserts one value at the given position.
- Notes: This is the second implemented `add(...)` form. It is also how empty nested lists can pick up type information in contexts like `grid.add([]);`.
- Complexity: O(n) in the number of shifted trailing elements

### Remove last element

- Syntax: `values.remove();`
- What it does: Removes the final element of a list.
- Notes: In `--run`, removing from an empty list reports a readable runtime error.
- Complexity: O(1)

### Remove one element at an index

- Syntax: `values.remove(index);`
- What it does: Removes the element at the given position.
- Notes: This is the indexed `remove(...)` form. Negative indexing follows the same model as indexed access.
- Complexity: O(n) in the number of shifted trailing elements

### Index access

- Syntax: `values[i]`
- What it does: Returns the element at index `i`.
- Notes: Negative indexing is supported, so `values[-1]` is the last element. In `--run`, out-of-range access reports a CP++ runtime error.
- Complexity: O(1)

### Index assignment

- Syntax: `values[i] = x;`
- What it does: Replaces one existing element.
- Notes: Uses the same indexing rules as read access.
- Complexity: O(1)

### Slicing

- Syntax: `values[start:end]`
- What it does: Returns a new list containing the half-open range `[start, end)`.
- Notes: Python-style slice syntax. Supported on list expressions and on `string`. Negative bounds are accepted. Bounds are clamped into the valid range, and if the resolved start is not before the resolved end, the result is an empty list.
- Complexity: O(k) for slice length `k`

### Concatenation

- Syntax: `a + b`
- What it does: Returns a new list containing the elements of `a` followed by the elements of `b`.
- Notes: Both sides must be compatible list types.
- Complexity: O(len(a) + len(b))

### In-place concatenation

- Syntax: `a += b`
- What it does: Appends the elements of `b` onto `a`.
- Notes: Still type-checked as an assignment-style update.
- Complexity: O(len(b))

### Sort in place

- Syntax: `values.sort();`
- What it does: Sorts a list in place using the default ordering of the element type.
- Notes: Takes no arguments.
- Complexity: O(n log n)

### Reverse in place

- Syntax: `values.reverse();`
- What it does: Reverses a list in place.
- Notes: Takes no arguments.
- Complexity: O(n)

### Length

- Syntax: `len(values)`
- What it does: Returns the number of elements in a list as an `int`.
- Notes: Also works on `string`.
- Complexity: O(1)

### Minimum of a list

- Syntax: `min(values)`
- What it does: Returns the smallest element in a list.
- Notes: In `--run`, calling it on an empty list reports a readable runtime error.
- Complexity: O(n)

### Maximum of a list

- Syntax: `max(values)`
- What it does: Returns the largest element in a list.
- Notes: In `--run`, calling it on an empty list reports a readable runtime error.
- Complexity: O(n)

### Minimum / maximum of multiple values

- Syntax: `min(a, b, c)`, `max(a, b, c)`
- What it does: Returns the smallest or largest value from a same-typed sequence of expressions.
- Notes: This is a separate implemented form from `min(list)` / `max(list)`. All items in the sequence must have the same CP++ type.
- Complexity: O(k) for `k` values

### Sum of a numeric list

- Syntax: `sum(values)`
- What it does: Returns the sum of a numeric list.
- Notes: Accepts `List<bool>`, `List<char>`, `List<int>`, and `List<float>`. Nested lists are rejected.
- Complexity: O(n)

### Membership test

- Syntax: `x in values`
- What it does: Returns whether an element appears in a list.
- Notes: If both sides are lists of the same type, CP++ performs sublist search. This also covers cases like `"hi" in words` when `words` is a `List<string>`.
- Complexity: O(n) for element membership; O(n + m) for sublist membership where `n` is the haystack length and `m` is the pattern length

### Find all matching indices

- Syntax: `values.find(3)`
- What it does: Returns a `List<int>` containing every zero-based index where `3` occurs.
- Notes: Also supports `values.find(sublist)` and returns starting indices of sublist matches. Works on `string` as well.
- Complexity: O(n) for single-element search; sublist search grows with container and pattern length

## Strings

### String indexing

- Syntax: `s[i]`
- What it does: Returns one character from a string.
- Notes: Shares list-style indexing behavior, including negative indices.
- Complexity: O(1)

### String slicing

- Syntax: `s[start:end]`
- What it does: Returns a sliced string value.
- Notes: Uses the same slice rules as lists.
- Complexity: O(k) for slice length `k`

### String append

- Syntax: `s.add(c);`
- What it does: Appends a character or compatible value to the string.
- Notes: `string` behaves like a built-in `List<char>` for these operations.
- Complexity: amortized O(1)

### String membership

- Syntax: `'a' in s`
- What it does: Checks whether a character or compatible pattern occurs in the string.
- Notes: Follows the list-style membership model. Implemented successful forms include both character membership like `'e' in s` and substring membership like `"ell" in s`.
- Complexity: O(n)

### String find

- Syntax: `s.find("ab")`
- What it does: Returns a `List<int>` of match positions.
- Notes: Works through the same `find(...)` surface documented for lists.
- Complexity: grows with string and pattern length

## Input

### Target-typed scalar input

- Syntax:
  ```cpp
  int x = input();
  x = input();
  ```
- What it does: Reads one value using the destination type.
- Notes: `input()` must currently be the entire right-hand side of a declaration or assignment.
- Complexity: O(1) plus input size

### Multi-target input

- Syntax:
  ```cpp
  int a, b, c = input();
  a, b, c = input();
  ```
- What it does: Reads one value per target.
- Notes: The target structure drives the read shape.
- Complexity: O(k) for `k` targets plus input size

### One-dimensional list input by count

- Syntax: `List<int> values = input(n);`
- What it does: Reads `n` elements into a one-dimensional list.
- Notes: The size expression must be an `int`.
- Complexity: O(n)

### Multi-dimensional list input

- Syntax: `List<List<int>> grid = input(rows, cols);`
- What it does: Reads nested list data in row-major order into the requested shape.
- Notes: Number of sizes should match list dimensionality.
- Complexity: O(total elements)

### One-dimensional line-style list input

- Syntax: `List<int> values = input();`
- What it does: Reads one line into a one-dimensional list target.
- Notes: Only supported for one-dimensional lists with no explicit sizes.
- Complexity: O(n)

### String word input

- Syntax: `string word = input();`
- What it does: Reads one whitespace-delimited string token.
- Notes: Standard string input form.
- Complexity: O(n)

### Fixed-length string input

- Syntax: `string chunk = input(3);`
- What it does: Reads exactly `n` characters.
- Notes: String input with explicit size accepts exactly one size argument.
- Complexity: O(n)

## Output and Introspection

### Print values

- Syntax: `print(a, b, c);`
- What it does: Prints one or more expressions.
- Notes: `string` values print as ordinary text, not bracketed character lists. `List` values print with bracketed formatting, including nested lists.
- Complexity: proportional to output size

### Empty print

- Syntax: `print();`
- What it does: Prints a newline.
- Notes: Useful for blank lines.
- Complexity: O(1)

### Custom line ending

- Syntax: `print("x", end = " ");`
- What it does: Prints values using a custom terminator instead of the default newline.
- Notes: `end` accepts a string, a char, or `flush`.
- Complexity: proportional to output size

### Flush output

- Syntax: `print("done", end = flush);`
- What it does: Emits a newline and flushes the stream.
- Notes: Older `flush`-as-final-argument syntax is documented as deprecated in `errors.txt`.
- Complexity: proportional to output size

### Describe any expression

- Syntax:
  ```cpp
  describe(x);
  describe(values[1]);
  describe(s.find("ab"));
  ```
- What it does: Prints a type-oriented description of an expression.
- Notes: This is implemented for more than plain variables: indexing, slicing, `find(...)`, arithmetic, and future expression forms flow through the same surface.
- Complexity: proportional to the described value's printed representation

## Truthiness and Conditions

### Bool-convertible conditions

- Syntax:
  ```cpp
  if (value) {
      ...
  }
  ```
- What it does: Uses CP++ truthiness rules in condition positions.
- Notes: Lists and strings participate in truthiness in the implemented language surface, which is why patterns like `if (values) { ... }` and `if (s) { ... }` work naturally.
- Complexity: O(1)

## Control Flow

### `if`

- Syntax:
  ```cpp
  if (cond) {
      ...
  }
  ```
- What it does: Executes a block when the condition is true.
- Notes: Conditions must be `bool` or convertible to `bool` under implemented rules.
- Complexity: O(1) control overhead

### `else if`

- Syntax:
  ```cpp
  else if (cond) {
      ...
  }
  ```
- What it does: Chains a secondary condition.
- Notes: Standard branching behavior.
- Complexity: O(1) control overhead

### `else`

- Syntax:
  ```cpp
  else {
      ...
  }
  ```
- What it does: Executes when preceding conditions fail.
- Notes: Standard branching behavior.
- Complexity: O(1) control overhead

### `while`

- Syntax:
  ```cpp
  while (cond) {
      ...
  }
  ```
- What it does: Repeats while the condition remains true.
- Notes: Supports loop `else`.
- Complexity: O(iterations)

### Classic `for`

- Syntax:
  ```cpp
  for (int i = 0; i < n; i++) {
      ...
  }
  ```
- What it does: Standard three-part counted loop.
- Notes: A variable declared in the initializer belongs to the loop scope.
- Complexity: O(iterations)

### Foreach `for (T x in list)`

- Syntax:
  ```cpp
  for (int x in values) {
      ...
  }
  ```
- What it does: Iterates over list elements in order.
- Notes: The right-hand side must be a list expression. Since `string` behaves like a built-in `List<char>`, `for (char c in s)` is also supported.
- Complexity: O(n)

### `rep(count)`

- Syntax:
  ```cpp
  rep(5) {
      ...
  }
  ```
- What it does: Repeats the block `count` times.
- Notes: A compact contest-oriented loop form.
- Complexity: O(count)

### `break`

- Syntax: `break;`
- What it does: Exits the nearest enclosing loop.
- Notes: Valid in classic `for`, foreach, `while`, and `rep`.
- Complexity: O(1)

### `continue`

- Syntax: `continue;`
- What it does: Skips to the next iteration of the nearest enclosing loop.
- Notes: Valid in classic `for`, foreach, `while`, and `rep`.
- Complexity: O(1)

### Loop `else`

- Syntax:
  ```cpp
  for (int x in values) {
      if (x == target) {
          break;
      }
  } else {
      print("not found");
  }
  ```
- What it does: Executes the `else` block only if the loop finishes normally without hitting `break`.
- Notes: Supported on classic `for`, foreach, `while`, and `rep`. In `--submit`, helper state is only kept when a loop actually has an `else`.
- Complexity: O(iterations)

## Runtime-Checked Behavior in `--run`

### List index bounds

- Syntax: `values[i]`, `values[i] = x`
- What it does: Checks bounds before indexed access/update.
- Notes: Reports readable runtime errors for invalid indices.
- Complexity: O(1)

### Empty remove

- Syntax: `values.remove();`
- What it does: Checks that the list is non-empty before removal.
- Notes: Only preserved as a readable diagnostic path in `--run`.
- Complexity: O(1)

### Empty `min` / `max`

- Syntax: `min(values)`, `max(values)`
- What it does: Checks that the list is non-empty.
- Notes: Reports CP++ runtime errors in `--run`.
- Complexity: O(n)

### Integer divide-by-zero and modulo-by-zero checks

- Syntax: `a / b`, `a % b`
- What it does: Checks dangerous integer cases in `--run`.
- Notes: `--submit` lowers these as ordinary C++ operations.
- Complexity: O(1)

### Integer overflow checks

- Syntax: integer `+`, `-`, `*`, and edge cases like `LLONG_MIN / -1`
- What it does: Preserves readable runtime checking for implemented dangerous integer cases in `--run`.
- Notes: This is part of the runtime-diagnostics layer, not a promise of full symbolic safety.
- Complexity: O(1)

## Useful Implemented Patterns

These are not separate language features, but the executable docs already show that the current language surface comfortably supports common competitive-programming patterns such as:

- prefix sums
- suffix scans
- stack simulation with `List`
- run-length encoding
- palindrome checks
- rotation and partition tasks
- simple string transforms such as Caesar shift
- membership- and slice-heavy list logic

That is the core idea of CP++ in practice: it builds in the data-shaping and control-flow conveniences, while leaving the actual algorithmic idea in the user's hands.
