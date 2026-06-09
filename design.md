# CP++ Frontend Design Notes

## Current State

The compiler currently parses one statement per source line and lowers CP++ into C++.
0
Supported s0tatement forms:

- primitive declarations
- assignments
- `print(...)`

Supported expression forms:

- literals
- identifiers
- parenthesized expressions
- explicit casts such as `(char)x`
- arithmetic operators `+`, `-`, `*`, `/`, `%`
- target-typed `input()` in declarations and assignments

The compiler tracks declared variable names and their CP++ types in a small symbol table.

## Numeric Type Model

The current implicit conversion hierarchy is:

```text
bool -> char -> int -> bigint
              \-> float -> bigfloat
```

`bigint` and floating-point types are separate numeric branches. Conversions between those branches require explicit casts.

Examples:

```cpp
int x = 5;
bigint y = x;      // implicit upcast

char c = '0';
int n = c;         // implicit upcast

char bad = x;      // invalid narrowing
char ok = (char)x; // explicit cast
```

## Current Input Design

`input()` is target-typed:

```cpp
int x = input();
x = input();
```

The declaration or assignment target decides the type to read. The current C++ lowering uses helper functions such as:

```cpp
CPPPInputInt()
CPPPInputChar()
CPPPInputBigInt()
```

This is acceptable as a temporary implementation detail, but it is not the long-term design. It does not scale nicely to future types such as:

```cpp
List<List<String>> values = input();
```

## Future Input Design

The frontend should eventually represent types as structured data instead of only enum values.

Proposed type representation:

```cpp
struct CpppTypeRef {
    std::string name;
    std::vector<CpppTypeRef> arguments;
};
```

Examples:

```text
int                  => { name: "int", arguments: [] }
bigint               => { name: "bigint", arguments: [] }
List<String>         => { name: "List", arguments: [String] }
List<List<String>>   => { name: "List", arguments: [List<String>] }
```

Once types are represented this way, `input()` should lower through a generic read protocol instead of a pile of type-specific frontend branches.

Desired generated shape:

```cpp
template <typename T>
T CPPPRead();

long long x = CPPPRead<long long>();
CPPPList<CPPPList<string>> values = CPPPRead<CPPPList<CPPPList<string>>>();
```

Or, if custom parsing metadata is needed:

```cpp
auto values = CPPPRead(typeDescriptorFor<List<List<String>>>());
```

The frontend should only need to answer:

```text
What is the target CP++ type of this input() expression?
What C++ type does that CP++ type lower to?
```

Then the runtime/library layer handles how to read that type.

## Planned Refactor Path

1. Introduce an AST type node.

   Replace `CpppType` enum-only handling with a `CpppTypeRef` structure. Keep enum helpers for primitive types during the transition.

2. Parse type syntax independently.

   Type parsing should handle primitive names first, then generic type names:

   ```text
   type := Identifier ('<' type (',' type)* '>')?
   ```

3. Store full type refs in the symbol table.

   The symbol table should become:

   ```cpp
   std::map<std::string, CpppTypeRef>
   ```

4. Make expressions carry type refs.

   `ExpressionEmitResult` should report a `CpppTypeRef`, not just a primitive enum.

5. Lower target-typed `input()` generically.

   Instead of `inputFunctionForType(CpppType)`, use:

   ```cpp
   std::string readExpressionForType(const CpppTypeRef& type);
   ```

   That function should generate:

   ```cpp
   CPPPRead<loweredCppType(type)>()
   ```

6. Move reading behavior into generated runtime support.

   Runtime support should define `operator>>` or `CPPPRead<T>()` specializations for primitive, numeric, string, and collection types.

## Design Rule

`input()` should remain target-typed in the language.

The compiler should not infer the shape of `input()` from context inside arbitrary expressions unless the expression itself has a clear target type.

Good:

```cpp
int x = input();
values = input();
```

Deferred:

```cpp
print(input());
int x = input() + 1;
```

Those may become legal later if the parser gains bidirectional type checking. For now, keeping `input()` target-typed avoids ambiguous reads and keeps diagnostics simple.
