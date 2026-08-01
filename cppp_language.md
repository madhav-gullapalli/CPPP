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

- Syntax: `build/cppp --cppp file.cppp --submit [--readable]`
- What it does: Emits compact contest-oriented generated C++ and prunes unreachable functions, unused container classes, runtime helpers, and individual container methods.
- Notes: By default, submit output removes comments, indentation, blank lines, and every token separator that C++ does not require. Preprocessor directives retain their required terminating newlines; the remaining C++ is emitted on one line. String and character literal contents are unchanged. Add `--readable` to inspect the same pruned submit program with ordinary formatting, for example `build/cppp --cppp file.cppp --submit --readable`. For the Make targets, use `make submit INPUT=file.cppp READABLE=1` or `make subrun INPUT=file.cppp READABLE=1`. Method overloads and comparison operators are retained independently; using `List.size()` does not retain unrelated `List` methods. Requirements inside unreachable functions do not keep methods or classes alive. Some extra runtime checking present in `--run` is intentionally not carried into submit-oriented output. Normal transpile and `--run` modes remain readable and retain the complete runtime support surface for diagnostics and debugging.
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

### `Stack<T>`, `Queue<T>`, and `Deque<T>`

- Syntax: `Stack<int> stack;`, `Queue<int> queue;`, `Deque<int> deque;`
- What it does: Declares an empty linear data structure with one element type.
- Notes: These values are heap-backed by CP++ smart pointers and alias on ordinary assignment. Stack lowers to C++ `stack`, Queue to C++ `queue`, and Deque to C++ `deque`.
- Complexity: O(1) construction

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

### Inferred `var` declaration

- Syntax: `var total = values[0] + 1;`
- What it does: Declares one variable whose type is inferred from its initializer.
- Notes: `var` requires exactly one initialized variable. It can infer scalar, string, container, pair, and range-expression result types, but cannot infer from `input()` or an empty container literal such as `[]` or `{}`.
- Complexity: O(1) plus initializer cost

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

### Set literals

- Syntax: `Set<int> seen = {3, 1, 2, 3};`
- What it does: Creates a set from brace-delimited values.
- Notes: Duplicate values are removed and iteration/printing are sorted. `{}` is contextual, so declare the target `Set<T>` type when creating an empty set.
- Complexity: O(n log n) for `n` literal values

### Map literals

- Syntax: `Map<int, int> counts = {2:7, 1:4};`
- What it does: Creates a map from brace-delimited `key:value` entries.
- Notes: Keys and values must match the declared map types. Nested literals are supported, for example `Map<int, Set<int>> groups = {1:{2, 3}};`.
- Complexity: O(n log n) for `n` entries

### Pair literals

- Syntax: `Pair<int, int> point = (4, 9);`
- What it does: Creates a two-value pair.
- Notes: Pair literals can be nested, for example `(1, (2, 3))`.
- Complexity: O(1) plus element construction

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

### Explicit scalar casts

- Syntax: `int("42")`, `char(65)`, `string(2.5)`
- What it does: Converts among implemented scalar types.
- Notes: `bool`, `char`, `int`, `float`, and `string` are cast functions, not C++-style `(Type)value` syntax. String-to-scalar casts validate their input and report CP++ runtime errors in `--run` for invalid text.
- Complexity: O(1), except proportional to string length for string parsing/formatting

### Container and range casts

- Syntax:
  ```cpp
  Set<int> seen = Set([4, 1, 4]);
  List<int> ordered = List(seen);
  Map<int, int> counts = Map([(3, 9), (1, 4)]);
  List<Pair<int, int>> entries = List(counts);
  List<int> values = List(range(5));
  ```
- What it does: Converts a List to a Set, a Set or Map to a List, a List of Pair values to a Map, and a range to a List or Set.
- Notes: Set and Map conversion results follow sorted iteration order. Container casts preserve compatible nested types and reject incompatible target types.
- Complexity: O(n log n) for set/map materialization; O(n) for list materialization

Lists also convert to and from the linear data structures:

```cpp
Stack<int> stack = Stack([1, 2, 3]);
Queue<int> queue = Queue([1, 2, 3]);
Deque<int> deque = Deque([1, 2, 3]);
List<int> values = List(stack);
```

Each conversion is O(n) and preserves printed List order. For Stack, the final
List element is the top; for Queue, the first List element is the top/front.

## Expressions and Operators

### Aliasing and `copy()`

- Syntax: `List<int> alias = values;`, `var independent = copy(values);`
- What it does: Ordinary assignment aliases Lists, strings, Stacks, Queues, Deques, Sets, Maps, and classes. Pairs and inline structs copy their fields. `copy(value)` returns an independent deep copy.
- Notes: Primitive assignment still copies the primitive value. Deep copying recursively duplicates nested containers and non-circular struct links. Constructing a new outer List from existing elements can intentionally make a shallow copy: for example, `[words[0], words[1]]` creates a new outer List while retaining aliases to the two strings.
- Complexity: O(1) for alias assignment; O(size of the reachable non-circular value) for `copy()`.

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
- Notes: Number of sizes should normally match list dimensionality. Nested list input may also omit the final dimension and read each innermost one-dimensional list with line-style input.
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

### Indexed input targets

- Syntax:
  ```cpp
  values[1] = input();
  grid[0][1] = input();
  ```
- What it does: Reads directly into indexed list elements, including nested list elements.
- Notes: This uses the same target-typed input model as declarations and plain assignments.
- Complexity: O(1) plus input size

## Output and Introspection

### Print values

- Syntax: `print(a, b, c);`
- What it does: Prints one or more expressions.
- Notes: `string` values print as ordinary text, not bracketed character lists. `List` values print with bracketed formatting, including nested lists.
- Complexity: proportional to output size

### Print with collection delimiter

- Syntax:
  ```cpp
  print([1, 2, 3], delim = " ")
  print("abc", delim = '-')
  ```
- What it does: Prints the top-level elements of a List or Set using a custom delimiter.
- Notes: Strings participate through their `List<char>` behavior, so delimiters also work on strings. Maps do not support `delim` because their printed entries already contain key/value punctuation.
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
- Notes: Supports loop `nobreak`.
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

### Foreach `for (T x in iterable)`

- Syntax:
  ```cpp
  for (int x in values) {
      ...
  }
  ```
- What it does: Iterates over collection or range elements in order.
- Notes: The right-hand side may be a List, string, Set, Map, or range expression. Maps yield `Pair<key, value>` values in key order, and `var` can infer the loop variable type.
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

### Loop `nobreak`

- Syntax:
  ```cpp
  for (int x in values) {
      if (x == target) {
          break;
      }
  } nobreak {
      print("not found");
  }
  ```
- What it does: Executes the `nobreak` block only if the loop finishes normally without hitting `break`.
- Notes: Supported on classic `for`, foreach, `while`, and `rep`. In `--submit`, helper state is only kept when a loop actually has a `nobreak`.
- Complexity: O(iterations)

## Functions

### Top-level function definition

- Syntax:
  ```cpp
  int square(int x){
      return x * x;
  }
  ```
- What it does: Declares a top-level user-defined function.
- Notes: Implemented parameter and return types use the normal CP++ type surface, including `void`, `List<...>`, nested lists, and `string`.
- Complexity: O(1) declaration overhead

### Function calls

- Syntax: `square(5)`, `dfs(adj, vis, 0);`
- What it does: Calls a user-defined function from either an expression or a standalone statement.
- Notes: Argument count and argument types are checked at compile time.
- Complexity: proportional to the function body

### Recursive functions

- Syntax:
  ```cpp
  void dfs(List<List<int>> adj, List<bool> vis, int u){
      ...
      dfs(adj, vis, v);
  }
  ```
- What it does: Allows ordinary recursive patterns such as DFS.
- Notes: Functions are top-level only in the current implemented surface.
- Complexity: depends on recursion depth and body

### `void` returns

- Syntax:
  ```cpp
  void stop(){
      return;
  }
  ```
- What it does: Returns early from a `void` function.
- Notes: Non-`void` functions must return a value.
- Complexity: O(1)

### Container parameters by reference

- Syntax: `void f(List<int> values, string s){ ... }`
- What it does: Passes container parameters by reference by default.
- Notes: Mutating a plain `List<...>` or `string` parameter mutates the caller-visible value.
- Complexity: O(1) call setup

### `copy` parameters

- Syntax: `void f(copy List<int> values, List<int> ref){ ... }`
- What it does: Passes the `copy` parameter as an independent recursive copy instead of an alias.
- Notes: `copy` is only valid before collection, string, and class parameters; it has the same recursive copy behavior as `copy()`.
- Complexity: proportional to the copied container size

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
- What it does: Returns the number of elements in a collection as an `int`.
- Notes: Works on `List`, `string`, `Set`, and `Map` values.
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

### Split by element or sublist

- Syntax:
  ```cpp
  values.split(2)
  values.split([2, 3])
  ```
- What it does: Breaks a list into a `List<List<T>>` using either one element or a same-typed sublist delimiter.
- Notes: Consecutive delimiters are skipped, so they do not produce empty pieces.
- Complexity: grows with list and delimiter length

## Stacks, Queues, and Deques

All three structures print as Lists and support O(1) `len(...)`.

### Stack and Queue

```cpp
Stack<int> stack;
stack.add(1);
int peeked = stack.top();
int removed = stack.pop();
```

Stack and Queue share the same CP++ interface. `add(value)` pushes onto a
Stack or enqueues at the back of a Queue. `top()` returns the Stack top or
Queue front. `pop()` returns and removes that same element. These operations
are O(1); `top()` and `pop()` report a runtime error on an empty structure.

### Deque

```cpp
Deque<int> deque;
deque.addFront(1);
deque.addBack(2);
int first = deque.front();
int last = deque.back();
deque.popFront();
deque.popBack();
```

`front()` and `back()` peek at either end. `addFront(value)` and
`addBack(value)` insert, while `popFront()` and `popBack()` return and remove.
All six operations are O(1); peeking or popping an empty Deque reports a
runtime error.

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

### String split

- Syntax:
  ```cpp
  "a--b--c".split("--")
  "a b c".split(' ')
  ```
- What it does: Splits a string through the same list-style `.split(...)` operation because `string` behaves like `List<char>`.
- Notes: The result is a `List<string>`.
- Complexity: grows with string and delimiter length

## Ranges

### Construct and iterate ranges

- Syntax:
  ```cpp
  for (int x in range(5)) { print(x); }
  for (int x in range(5, 1)) { print(x); }
  for (int x in range(2, 11, 3)) { print(x); }
  ```
- What it does: Iterates `[0, stop)`, `[start, stop)` with an inferred direction, or a stepped sequence.
- Notes: `range(start, start)` is empty. Range arguments must be `int` values, and a three-argument range rejects zero steps.
- Complexity: O(iterations)

### Range membership and materialization

- Syntax: `x in range(...)`, `List(range(...))`, `Set(range(...))`
- What it does: Tests whether an integer belongs to a range or materializes its values into a collection.
- Notes: Membership is arithmetic and does not build an intermediate list. `List(range)` and `Set(range)` always produce `int` elements.
- Complexity: O(1) membership; O(n) List materialization; O(n log n) Set materialization

## Sets, Maps, and Pairs

### Set operations

- Syntax:
  ```cpp
  Set<int> seen = {3, 1, 2};
  seen.add(4);
  print(2 in seen, seen.remove(1));
  ```
- What it does: Adds, removes, and checks membership of unique sorted values.
- Notes: `print(seen, delim = ",")` prints the values in sorted order. `min(set)`, `max(set)`, `set.prev(x)`, and `set.next(x)` expose ordered traversal operations.
- Complexity: O(log n) add/remove/membership; O(1) min/max; O(log n) prev/next

### Map operations

- Syntax:
  ```cpp
  Map<int, int> counts = {2:7, 1:4};
  counts[3] = 9;
  print(counts[1], counts.at(2), 3 in counts);
  ```
- What it does: Reads and updates values by sorted keys.
- Notes: `map[key]` inserts a default value if the key is missing. `map.at(key)` requires an existing key, while `map.remove(key)` returns and removes one. `min(map)`, `max(map)`, `map.prev(key)`, and `map.next(key)` operate on keys.
- Complexity: O(log n) access, update, membership, remove, prev, and next; O(1) min/max

### Pair access and map iteration

- Syntax:
  ```cpp
  Pair<int, int> point = (4, 9);
  point[0] += 1;
  for (Pair<int, int> entry in counts) {
      print(entry[0], entry[1]);
  }
  ```
- What it does: Accesses mutable pair elements and iterates a map as sorted `Pair<key, value>` entries.
- Notes: Pair indexes are exactly `0` and `1`. Foreach also supports `Set` values and `for (var entry in counts)` inference.
- Complexity: O(1) pair access; O(n) traversal

## Classes and structs

### Class declaration

- Syntax:
  ```cpp
  class Point {
      int x;
      int y;
      void shift(int amount) {
          x += amount;
          y += amount;
      }
  }
  ```
- What it does: Declares a public CP++ class with typed fields and typed methods.
- Notes: CP++ methods refer to fields directly by name; `self` is not used. Class fields and methods are public.
- Complexity: Declaration-time cost is proportional to the number of fields and methods.

### Class construction and default values

- Syntax: `Point p = Point(2, 3);`, `Point empty;`
- What it does: Constructs a class from its fields, or declares a null class when no constructor value is supplied.
- Notes: A constructed class lowers to CP++'s reference-counted pointer handle. A default-initialized class is `NULL`. Constructor arguments must match field types and declaration order. Acyclic composite values are reclaimed automatically when their final alias is released; cycles remain allocated.
- Complexity: O(number of fields), excluding recursive container-copy cost.

### Class fields

- Syntax: `p.x`, `p.x = 7`
- What it does: Reads and writes public class fields.
- Notes: Class fields can be primitive values, containers, classes, or structs. Recursive fields are allowed because class-valued fields use owned pointer representation.
- Complexity: O(1) for a direct field access, excluding the value being assigned.

### Class methods

- Syntax:
  ```cpp
  p.shift(1);
  ```
- What it does: Calls a method declared inside the class.
- Notes: Method arguments must match their declared types. Methods may read and update the receiver's fields directly. Calling a method on `NULL` is a runtime error.
- Complexity: The complexity of the method body.

In `--submit` output, reachability is tracked at function granularity. Unused
class methods, entirely unused classes, and unreachable top-level functions
are omitted; methods and functions reached from retained code remain available.

### Class aliasing and reassignment

- Syntax: `Point alias = original;`, `alias = original;`
- What it does: Makes both names refer to the same class value.
- Notes: Mutating fields through either alias is visible through the other. Use `copy(original)` when an independent value is required.
- Complexity: O(1).

### Recursive classes

- Syntax:
  ```cpp
  class Node {
      int value;
      Node next;
  }
  Node tail = Node(2, NULL);
  Node head = Node(1, tail);
  ```
- What it does: Allows a class to contain a field of its own type or another custom type.
- Notes: Recursive fields default to `NULL`; recursive construction, field access, explicit copying, and nested printing are supported. Class links use CP++'s reference-counted pointer handle, so structures can contain back-links and cycles, including doubly linked lists. Acyclic links are reclaimed automatically; cycles are intentionally not collected. Printing, equality, or deep-copying a cycle recursively is not cycle-terminating; operate on its fields or break the cycle first.
- Complexity: Proportional to the traversed recursive structure.

### Class printing

- Syntax: `print(point);`
- What it does: Prints a class as JSON-like named fields, in declaration order.
- Notes: Nested classes use nested braces, containers retain their normal formatting, and null class values print as `NULL`.
- Complexity: O(size of the printed object).

### Class equality

- Syntax: `a == b`, `a != b`, `a == NULL`
- What it does: Compares all fields recursively, including nested classes and containers.
- Notes: Two null classes compare equal. Ordering comparisons on classes are not supported. `=` is assignment; use `==` for equality.
- Complexity: O(size of the compared objects) in the worst case.

### Class function parameters

- Syntax:
  ```cpp
  void rotate(Point point) {
      int oldX = point.x;
      point.x = -point.y;
      point.y = oldX;
  }
  ```
- What it does: Passes a class to a function so the function can inspect and update its fields.
- Notes: Class parameters share the same referenced object in generated C++, while CP++ source uses ordinary class syntax.
- Complexity: O(1) to pass the reference; operations inside the function determine total cost.

### Inline struct declaration

- Syntax: `struct Point { int x; int y; }`
- What it does: Declares an inline value type with public fields and methods.
- Notes: Struct fields may use primitives, `List`, `Set`, `Map`, and `Pair` of non-custom values. A struct cannot contain a struct, a class, itself, or a collection containing either; use `class` for those layouts. Structs are never `NULL`, do not use reference-counted handles, and ordinary assignment copies their fields.

### Inline struct construction, fields, and methods

- Syntax: `Point p = Point(2, 3);`, `p.x = 7`, `p.shift(1)`
- What it does: Constructs and operates on an inline struct value.
- Notes: A default-initialized struct contains default values for its fields. Struct parameters are passed as values, so mutating a normal parameter does not mutate the caller's struct. Struct equality compares fields; `NULL` comparisons are not supported.

### `range`

- Syntax: `range values = range(stop);`, `range(start, stop)`, or `range(start, stop, step)`
- What it does: Represents a lazy integer sequence. One-argument ranges start at zero; two-argument ranges choose an ascending or descending unit step; three-argument ranges use the supplied non-zero step magnitude in the required direction.
- Notes: Ranges support foreach iteration, integer membership, truthiness through emptiness, and conversion to `List<int>` or `Set<int>`.
- Complexity: O(1) storage and O(1) membership

### `Set<T>`

- Syntax: `Set<int> seen;`
- What it does: Stores unique values in sorted order.
- Notes: `Set` requires exactly one subtype. Nested collection values such as `Set<List<int>>` are supported when the element type is orderable.
- Complexity: O(1) for the type itself; O(log n) insert, remove, and membership

### `Map<K, V>`

- Syntax: `Map<int, int> counts;`
- What it does: Stores sorted key/value entries.
- Notes: `Map` requires exactly two subtypes. Bracket access creates a default value for a missing key; use `at(key)` when the key must already exist.
- Complexity: O(1) for the type itself; O(log n) lookup and update

### `Pair<A, B>`

- Syntax: `Pair<int, int> point;`
- What it does: Stores two values, accessible at indexes `0` and `1`.
- Notes: `Pair` requires exactly two subtypes and can be nested. Pairs are inline values, so ordinary assignment copies the two fields. Use `copy(pair)` when nested composite fields also need independent deep copies.
- Complexity: O(1)

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
