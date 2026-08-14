# CP++ Semantic Surface Inventory

This is a decision worksheet, not a language specification. It records the syntax-only information currently delivered by `ProgramAst`, the facts the existing combined checker/code generator currently derives, and the policy questions that must be answered before semantic analysis is separated. Repository code and tests are the source of truth.

AST trees below omit ownership notation and token contents when the parsed expression already carries the same structure. Every `ProgramStatement` also has `kind`, `syntax` (`sourceSpan`, line/columns, tokens, `terminated`, `opensBlock`, `codeLength`), `syntaxOk`, `syntaxErrorOffset`, and `syntaxError`. Every `Expr` also has `sourceColumn`, `sourceSpan`, `inferredType`, `mutableValue`, and `explicitCast`; the last three are semantic fields currently filled by `ExpressionAnalyzer`, not syntax facts. Empty/default fields are shown where they affect the question. Spans are abbreviated when their exact offsets do not matter.

To keep 78 cases readable, later trees use exact abbreviations established by the first full examples: `Int("1")` means `LiteralExpr(kind=Int,text="1")`; `String("x")` and `Null("NULL")` expand the same way; `type=int` means `type=TypeSyntax(name="int",arguments=[],functionType=false,syntaxOk=true)`; `int x` in a parameter list means `ParameterSyntax(type=TypeSyntax("int"),name="x",copyParameter=false,modifier="")`; and `args` means the actual `arguments` field. No omitted inherited field varies from the shared defaults above unless the tree says so.

## Part 1 — AST Semantic Inventory

### Shared syntax and program nodes

| Node | Actual semantically relevant fields | Information semantic analysis must derive or validate |
|---|---|---|
| `ProgramAstNode` | `sourceSpan` | Diagnostic ownership. |
| `SyntaxSite` | `sourceSpan`, line/start/end positions, `commentText` | Diagnostic locations; comments have no type meaning. |
| `StatementSyntax` | all `SyntaxSite` fields, `tokens`, `terminated`, `opensBlock`, `codeLength` | Statement termination/recovery status; tokens remain compatibility input to current emitters. |
| `TypeSyntax` | spans, `spelling`, `name`, `syntaxError`, `arguments`, `functionParameters`, `functionParameterCopy`, `functionType`, `syntaxOk` | Type-name lookup, generic arity, nested validity, function signature, copy modes, and legality of `void`. |
| `ParameterSyntax` | span, `type`, `name`, `copyParameter`, `modifier`, `modifierSpan` | Parameter symbol, uniqueness, type, and copy-mode eligibility. |
| `ProgramStatement` | `kind`, `syntax`, `syntaxOk`, `syntaxErrorOffset`, `syntaxError` | Whether to continue after recovery and which context permits the statement. |
| `BlockAst` | `statements`, `closingSyntax`, `hasClosingSyntax` | Lexical scope, declaration lifetime, contextual nesting, and recovery at a missing close brace. |
| `ProgramAst` | `body` | File scope, declaration order, and global namespace state. |

### Statement nodes

| Node | Own fields | Semantic work |
|---|---|---|
| `CommentStatementAst` | none beyond base | None. |
| `ErrorStatementAst` | `reason`, optional `recoveredBody` | Cascade policy and whether recovered children are analyzed. |
| `VariableDeclarationAst` | `type`, `inferredType`, `names`, `nameSpans`, `initializers`, `continuationTokenIndex` | Name insertion time, type inference/resolution, default initialization, arity, conversions, duplicates. |
| `AssignmentStatementAst` | `operation`, operation span/token, `targets`, `values`, target/value token slices and offsets | Lvalue checks, operator compatibility, conversions, multi-assignment evaluation and arity. |
| `ExpressionStatementAst` | `expression` | Whether a value/call/mutation is legal as a statement. |
| `ReturnStatementAst` | optional `value`, `valueTokens`, `valueOffset` | Function context, return presence/type/conversion, path completeness. |
| `SimpleControlStatementAst` | base `kind` is `Break` or `Continue` | Nearest-loop context. |
| `ConditionalBranchAst` | header syntax, condition plus tokens/offset, syntax recovery fields, `body` | Bool conversion and branch-local scope. |
| `CompletionBranchAst` | header syntax, `body` | Association with a loop/if and its scope. |
| `IfStatementAst` | condition plus tokens/offset, `thenBody`, `elseIfBranches`, optional `elseBranch` | All branch conditions/scopes and flow facts. |
| `WhileStatementAst` | condition plus tokens/offset, `body`, optional `nobreakBranch` | Condition, loop control, completion semantics. |
| `ForClauseAst` | `kind`, declaration fields, assignment fields, `expressions`, `tokens`, `offset` | Per-kind validation and loop-scope effects. |
| `ForStatementAst` | `initializer`, condition plus tokens/offset, `iteration`, `body`, optional `nobreakBranch` | Header order/scope, condition, iteration, completion. |
| `ForEachStatementAst` | `variableType`, `variableName`, `inferredVariable`, `iterable` plus tokens/offsets, `body`, optional `nobreakBranch` | Iterable element type, loop variable conversion/scope/mutation. |
| `RepStatementAst` | `count` plus tokens/offset, `body`, optional `nobreakBranch` | Numeric count conversion and loop context. |
| `FunctionDeclarationAst` | `returnType`, `name`, `nameSpan`, `parameters`, `body` | Signature registration, collisions, recursion/order, parameter scope, return analysis. |
| `AggregateDeclarationAst` | `name`, `nameSpan`, `isClass`, `body` | Type registration, permitted members, field/method tables, class/struct rules. |

### Expression nodes

| Node | Own fields | Semantic work |
|---|---|---|
| `Expr` | column/span; semantic `inferredType`, `mutableValue`, `explicitCast` | Produce a type and value category, preserving explicit-cast provenance where policy needs it. |
| `ErrorExpr` | `reason` | Cascade/recovery policy. |
| `LiteralExpr` | `kind` (`Bool`, `Null`, `Int`, `Float`, `String`, `Char`), `text` | Literal type/range and contextual typing, especially `NULL`. |
| `VariableExpr` | `name` | Namespace lookup, declaration order, function-value resolution, mutability. |
| `FieldExpr` | `base`, `field` | Aggregate/field lookup, field type and lvalue propagation. |
| `UnaryExpr` | `op`, `operand`, `postfix` | Operator domain/result; mutable operand for increment/decrement. |
| `BinaryExpr` | `op`, `left`, `right` | Operator family, promotion/conversion, result, container/custom behavior. |
| `CastExpr` | semantic `targetType`, `operand` | Cast legality and result. Notably the current syntax AST already stores a resolved `Type` here. |
| `CallExpr` | `callee`, semantic `functionType`, optional `receiver`, `arguments`, `argumentNames`, semantic `partialApplication` | Callable/method/builtin/constructor resolution, argument rules, return or remaining function type. |
| `IndexExpr` | `base`, `index` | Indexability, key/index type, result and lvalue status. |
| `SliceExpr` | `base`, `start`, `end` | Sliceability, bound typing/defaults, result/value category. |
| `ListLiteralExpr` / `SetLiteralExpr` | `elements` | Empty/contextual typing and element unification. |
| `MapLiteralEntry` / `MapLiteralExpr` | `key`, `value`; `entries` | Key/value inference and unification. |
| `PairLiteralExpr` | `first`, `second` | Component types and contextual conversion. |

### Existing semantic side tables

`Type` contains `primitive`, `subtypes`, function `functionParameterCopy`, and custom-type `name`. `FunctionParameter` contains `name`, resolved `type`, `copyParameter`, and diagnostic `column`; `FunctionSignature` contains `name`, `returnType`, `returnsVoid`, and parameters. `CompileContext` currently owns separate maps for variables, functions, aggregate fields/orders/methods, plus a class-name set and stacks for blocks, loop break flags, and declared names. These are current implementation structures, not automatically the desired future namespace model.

## Part 2 — Semantic Rule Cases

### Program, scopes, and type syntax

#### CASE 1 — Ordered top-level statements

SOURCE
======
```cpp
int x = 1;
print(x);
```

AST
===
```text
ProgramAst
└── body: BlockAst(hasClosingSyntax=false)
    ├── VariableDeclarationAst(type=TypeSyntax(name="int"), inferredType=false,
    │   names=["x"], initializers=[LiteralExpr(kind=Int,text="1")])
    └── ExpressionStatementAst
        └── expression: CallExpr(callee="print", receiver=null,
            arguments=[VariableExpr(name="x")], argumentNames=[""])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
The analyzer receives source order. When is `x` introduced, and which constructs are legal at file scope?

CURRENT BEHAVIOR: Statements are lowered in order; variables are visible only after successful declaration. Ordinary executable top-level statements become generated `main` statements.

ANSWER
======
Variables should be visible whenever they are in scope AFTER THEY ARE introduced. 

However if a variable is in scope and introduced AFTER it is used. It would be nice to give a special error for that rather than just say the variable is undeclared as you currently do.
Give a hint to tell the user to declare the variable first
#### CASE 2 — Nested scope, shadowing, and escape

SOURCE
======
```cpp
int x = 1;
if (true) { int x = 2; int y = x; }
print(y);
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst(type=int,names=["x"],initializers=[Int("1")])
├── IfStatementAst
│   ├── condition: LiteralExpr(Bool,"true")
│   ├── thenBody: BlockAst
│   │   ├── VariableDeclarationAst(type=int,names=["x"],initializers=[Int("2")])
│   │   └── VariableDeclarationAst(type=int,names=["y"],initializers=[VariableExpr("x")])
│   ├── elseIfBranches=[]
│   └── elseBranch=null
└── ExpressionStatementAst(CallExpr("print",args=[VariableExpr("y")]))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
May a block shadow an outer name? Do branch locals disappear after the branch? Which declaration does an identifier bind to?

CURRENT BEHAVIOR: One visible-variable map rejects `x` as already declared even in the inner block; newly declared block names are erased on exit, so `y` is unavailable later.

ANSWER
======
In this this shoudl error as y is not declared in the scope. 
Though again it would be helpful to give a special error message to show that y is declared in a different scope. Maybe give a hint but I am struggl;ing to what will be usefull here. 
This. only applies in the case that an inner block declares the variable. I dont want random functions corrupoting suggestions.
#### CASE 3 — Context-only statements

SOURCE
======
```cpp
break;
continue;
return 1;
```

AST
===
```text
ProgramAst.body
├── SimpleControlStatementAst(kind=Break)
├── SimpleControlStatementAst(kind=Continue)
└── ReturnStatementAst(value=LiteralExpr(Int,"1"), valueOffset=7)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which enclosing construct supplies loop/function context, and should invalid context suppress child analysis?

CURRENT BEHAVIOR: `break`/`continue` require a nearest loop; `return` requires a function.

ANSWER
======
There are already LoopAST and funcAST nodes in the arsenal, siomply check if one of the parent nodes is a valid AST. 
Just say that these cannot be declared outside of <whatevere is correct> scope
#### CASE 4 — Primitive and string types

SOURCE
======
```cpp
bool b; char c; int i; float f; string s; range r;
```

AST
===
```text
ProgramAst.body
└── six VariableDeclarationAst nodes, each with
    type: TypeSyntax(spelling/name="bool"|"char"|"int"|"float"|"string"|"range",
                     arguments=[],functionType=false,syntaxOk=true)
    inferredType=false; names=[corresponding name]; initializers=[]
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which names are built-in types, are aliases canonicalized, and which positions permit each type?

CURRENT BEHAVIOR: These names resolve; `string` resolves to `List<char>`. `vector`, `set`, `map`, and `pair` also alias capitalized container types.

ANSWER
======
List<char> and string should work the same, but otherwise aliases are not aliased. 
i.e vector<int> should give an error. 

#### CASE 5 — Generic arity and nesting

SOURCE
======
```cpp
List<List<int>> grid;
Map<int, Pair<string, float>> table;
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst
│   └── type: TypeSyntax(name="List",arguments=[
│       TypeSyntax(name="List",arguments=[TypeSyntax(name="int",arguments=[])])])
└── VariableDeclarationAst
    └── type: TypeSyntax(name="Map",arguments=[TypeSyntax(name="int"),
        TypeSyntax(name="Pair",arguments=[TypeSyntax(name="string"),TypeSyntax(name="float")])])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Must every nested name resolve before the outer type? What arity and invalid-subtype recovery rules apply?

CURRENT BEHAVIOR: `List/Stack/Queue/Deque/Heap/Set` require one subtype; `Map/Pair` require two; primitive/string/custom types require zero; `void` is rejected in type arguments.

ANSWER
======
It woukld ideal to go inside out. Otherwise continue with what alrerady exists.
#### CASE 6 — Function type syntax and parameter modes

SOURCE
======
```cpp
int(copy List<int>, float) operation;
```

AST
===
```text
ProgramAst.body
└── VariableDeclarationAst
    ├── type: TypeSyntax(name="int",arguments=[],functionType=true,
    │   functionParameters=[TypeSyntax(name="List",arguments=[TypeSyntax("int")]),
    │                       TypeSyntax(name="float")],
    │   functionParameterCopy=[true,false])
    ├── inferredType=false; names=["operation"]
    └── initializers=[]
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Is the leading type the return type? Which parameter modifiers are valid, and are modes part of type identity?

CURRENT BEHAVIOR: It resolves to `Function<[return, parameters...]>`; copy flags are compared as part of function-type equality. `deep` remains accepted in function-type syntax as a copy-marked mode in current tests.

ANSWER
======
What you have is fine. 
#### CASE 7 — Unknown syntactic type in a known declaration shape

SOURCE
======
```cpp
def f(int x) { return x; }
```

AST
===
```text
ProgramAst.body
└── FunctionDeclarationAst
    ├── returnType: TypeSyntax(spelling="def",name="def",arguments=[],functionType=false,syntaxOk=true)
    ├── name="f"; nameSpan=<f>; parameters=[ParameterSyntax(type=TypeSyntax("int"),
    │   name="x",copyParameter=false,modifier="")]
    └── body: BlockAst(statements=[ReturnStatementAst(value=VariableExpr("x"))],hasClosingSyntax=true)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
The parser has committed to a function. Is `def` known; should `f` or `x` still be registered; should the body be checked after failure?

CURRENT BEHAVIOR: Type resolution reports `unsupported type def`; the header fails and the body is not lowered.

ANSWER
======
I tested this myslef. This is fine.
#### CASE 8 — User-defined and self-referential types

SOURCE
======
```cpp
class Node { Node next; }
Node head;
```

AST
===
```text
ProgramAst.body
├── AggregateDeclarationAst(name="Node",isClass=true)
│   └── body: BlockAst(statements=[VariableDeclarationAst(
│       type=TypeSyntax("Node"),names=["next"],initializers=[])])
└── VariableDeclarationAst(type=TypeSyntax("Node"),names=["head"],initializers=[])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
When is the type name registered? Which self/nested references are legal for structs versus classes?

CURRENT BEHAVIOR: The aggregate name is registered before members. Classes may contain custom types, including themselves; inline structs reject fields containing custom types.

ANSWER
======
Classes are fine, 

We will do a MAJOR architectural change to structs. 
Now that semantic analysis is a step. 
Structs should act like C structs. So, they can contain other structs, and heck even List<self>. 
This means structs should detetc cycles amogst ONLY structs. 
i.e
Class A{
    B b;
}
Struct B{
    A a;
} is fine since B is not part of the struct reference graph.
To do this make a directed graph and detetct if there is a cycle.
#### CASE 9 — `void` by position

SOURCE
======
```cpp
void f(void x) { void y; }
```

AST
===
```text
ProgramAst.body
└── FunctionDeclarationAst(returnType=TypeSyntax("void"),name="f",
    parameters=[ParameterSyntax(type=TypeSyntax("void"),name="x")],
    body=[VariableDeclarationAst(type=TypeSyntax("void"),names=["y"])])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
In which positions is `void` legal, and should later parts be analyzed after an invalid parameter?

CURRENT BEHAVIOR: `void` is allowed for a function return type but rejected for parameters, variables, generic arguments, and foreach variables.

ANSWER
======
Keep as is
### Declarations, assignments, identifiers, and literals

#### CASE 10 — Typed declaration with default initialization

SOURCE
======
```cpp
int count;
List<int> values;
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst(type=TypeSyntax("int"),inferredType=false,
│   names=["count"],nameSpans=[<count>],initializers=[],continuationTokenIndex=2)
└── VariableDeclarationAst(type=TypeSyntax("List",arguments=[TypeSyntax("int")]),
    inferredType=false,names=["values"],initializers=[],continuationTokenIndex=4)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which types may omit an initializer, what default is used, and is the name visible during initialization?

CURRENT BEHAVIOR: Typed declarations receive type-specific defaults; names are entered only after successful declaration emission.

ANSWER
======
Your behavior is fine
#### CASE 11 — Typed initializer and conversion

SOURCE
======
```cpp
float value = 3;
```

AST
===
```text
ProgramAst.body
└── VariableDeclarationAst(type=TypeSyntax("float"),inferredType=false,names=["value"],
    initializers=[LiteralExpr(kind=Int,text="3")],continuationTokenIndex=<=>)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
What is the initializer type, is implicit conversion permitted, and does an explicit cast bypass implicit rules?

CURRENT BEHAVIOR: `int` implicitly converts to `float`; declarations use the common conversion helpers, with special literal/container/input paths.

ANSWER
======
Keep all cast rules the same. 
#### CASE 12 — Inferred declaration

SOURCE
======
```cpp
var answer = 40 + 2;
```

AST
===
```text
ProgramAst.body
└── VariableDeclarationAst(type=TypeSyntax(),inferredType=true,names=["answer"],
    initializers=[BinaryExpr(op="+",left=LiteralExpr(Int,"40"),right=LiteralExpr(Int,"2"))])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Is an initializer mandatory, which types may be inferred, and how are ambiguous/empty/`input` values treated?

CURRENT BEHAVIOR: `var` requires an inferable initializer; it can infer function values and partial applications but rejects bare target-typed `input()` and empty untyped literals.

ANSWER
======
Keep var the same
#### CASE 13 — Multiple declaration

SOURCE
======
```cpp
int a, b = 1, 2;
```

AST
===
```text
ProgramAst.body
└── VariableDeclarationAst(type=TypeSyntax("int"),inferredType=false,
    names=["a","b"],nameSpans=[<a>,<b>],
    initializers=[LiteralExpr(Int,"1"),LiteralExpr(Int,"2")],continuationTokenIndex=<=>)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Must value count match name count, may names duplicate one another/existing names, and is evaluation simultaneous?

CURRENT BEHAVIOR: Multiple comma values must match the names. Any visible redeclaration is rejected; all declared names get the same resolved target type.

ANSWER
======
Keep same for now but keep in mind behaviort here is likely to change soon. 
#### CASE 14 — Sized/container/custom declaration forms

SOURCE
======
```cpp
List<int> values(5);
Point point = Point(1, 2);
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst(type=TypeSyntax("List",[int]),names=["values"],
│   initializers=[LiteralExpr(Int,"5")], continuationTokenIndex=<(>)
└── VariableDeclarationAst(type=TypeSyntax("Point"),names=["point"],
    initializers=[CallExpr(callee="Point",receiver=null,args=[Int("1"),Int("2")])])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
How does declaration syntax distinguish size initialization from an expression call, and what contextual target type is available?

CURRENT BEHAVIOR: Size initialization is specialized for `List`/`string`; other container size forms are rejected. Custom initialization is checked as a constructor call.

ANSWER
======
Keep this the same
#### CASE 15 — Simple assignment

SOURCE
======
```cpp
x = value;
```

AST
===
```text
ProgramAst.body
└── AssignmentStatementAst(operation="=",operationSpan=<=>,
    targets=[VariableExpr(name="x")],values=[VariableExpr(name="value")],
    targetTokens=[[x]],valueTokens=[[value]],targetOffsets=[0],valueOffsets=[4])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Does the target resolve and remain mutable; what conversion applies; when are target/value evaluated?

CURRENT BEHAVIOR: The target must be a mutable variable/field/index; the value must be implicitly convertible unless explicitly cast.

ANSWER
======
Keep as is
#### CASE 16 — Compound assignment families

SOURCE
======
```cpp
x += y; x -= y; x *= y; x /= y; x %= y;
```

AST
===
```text
ProgramAst.body
└── five AssignmentStatementAst nodes, each operation one of "+=","-=","*=","/=","%=",
    operationToken=<operator>,targets=[VariableExpr("x")],values=[VariableExpr("y")],
    with corresponding token slices/offsets
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Is compound assignment defined as target binary-op value plus assignment, with what evaluation/conversion rules?

CURRENT BEHAVIOR: Except direct-list `+=`, lowering synthesizes the corresponding binary expression and then assigns. Direct-list `+=` appends a same-typed list. Multi-target compound assignment is not accepted.

ANSWER
======
Keep as is
#### CASE 17 — Field and index assignment targets

SOURCE
======
```cpp
obj.field = 1;
values[i] = 2;
```

AST
===
```text
ProgramAst.body
├── AssignmentStatementAst(operation="=",targets=[FieldExpr(base=VariableExpr("obj"),field="field")],
│   values=[LiteralExpr(Int,"1")],targetTokens/valueTokens/offsets=<source slices>)
└── AssignmentStatementAst(operation="=",targets=[IndexExpr(base=VariableExpr("values"),
    index=VariableExpr("i"))],values=[LiteralExpr(Int,"2")],token slices/offsets=<source>)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
How is lvalue status propagated from the base, and do class, struct, list, map, and pair elements differ?

CURRENT BEHAVIOR: A valid field/index inherits base mutability. Temporaries are not assignable; list/map/pair indexing has its normal type restrictions.

ANSWER
======
Keep as is
#### CASE 18 — Multi-assignment

SOURCE
======
```cpp
a, b = b, a;
```

AST
===
```text
ProgramAst.body
└── AssignmentStatementAst(operation="=",targets=[VariableExpr("a"),VariableExpr("b")],
    values=[VariableExpr("b"),VariableExpr("a")],targetTokens=[[a],[b]],valueTokens=[[b],[a]],
    targetOffsets=[0,3],valueOffsets=[7,10])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Must arities match, are all values evaluated before writes, and is conversion per target?

CURRENT BEHAVIOR: Arity must match (except one `input()` can feed all targets); values are converted into temporaries before any assignment.

ANSWER
======
Same as declaration
#### CASE 19 — Identifier resolution and declaration order

SOURCE
======
```cpp
int x = y;
int y = 1;
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst(type=int,names=["x"],initializers=[VariableExpr(name="y")])
└── VariableDeclarationAst(type=int,names=["y"],initializers=[LiteralExpr(Int,"1")])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which namespace is searched, is forward variable reference legal, and does a failed declaration reserve `x`?

CURRENT BEHAVIOR: Variables require prior declaration; functions and selected builtins may resolve as function values. Failed variables can be remembered with `Unknown` to suppress cascades.

ANSWER
======
Keep as is, also see 1 for how to best report this.
#### CASE 20 — Function reference versus variable

SOURCE
======
```cpp
int add(int a, int b) { return a + b; }
var operation = add;
```

AST
===
```text
ProgramAst.body
├── FunctionDeclarationAst(returnType=int,name="add",parameters=[int a,int b],
│   body=[ReturnStatementAst(BinaryExpr("+",VariableExpr("a"),VariableExpr("b")))])
└── VariableDeclarationAst(inferredType=true,names=["operation"],
    initializers=[VariableExpr(name="add")])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Do variable and function names share a namespace, which wins on collision, and are function references first-class?

CURRENT BEHAVIOR: Variable lookup is attempted before function lookup. Functions are first-class values with a derived function `Type`; variables and functions are stored separately.

ANSWER
======
Change this. functions and variables are in the smae namespace. With closures and function variables the division betweens functions and variables is a bit arbritary.
#### CASE 21 — Scalar literals and overflow

SOURCE
======
```cpp
true; 42; 1.5; 'x'; "hi";
```

AST
===
```text
ProgramAst.body
└── five ExpressionStatementAst nodes containing LiteralExpr:
    (Bool,"true"), (Int,"42"), (Float,"1.5"), (Char,"'x'"), (String,"\"hi\"")
```

SEMANTIC INFORMATION / QUESTIONS
===============================
What type/range does each literal have, are pure literal statements legal, and does context alter typing?

CURRENT BEHAVIOR: Types are `bool`, `int`, `float`, `char`, and `List<char>` (`string`). Typed integer declaration checks `long long` overflow; malformed char literals have specialized diagnostics. Pure expressions are generally emitted if parsed.

ANSWER
======
Keep as is
#### CASE 22 — Null literal

SOURCE
======
```cpp
Node empty;
empty == NULL;
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst(type=TypeSyntax("Node"),names=["empty"],initializers=[])
└── ExpressionStatementAst
    └── BinaryExpr(op="==",left=VariableExpr("empty"),
        right=LiteralExpr(kind=Null,text="NULL",inferredType=Unknown,mutableValue=false))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Is `NULL` intrinsically typed or target-typed, and where may it occur?

CURRENT BEHAVIOR: The literal initially has `Unknown`. Aggregate constructor arguments specially accept it for class-typed fields, and comparisons permit it only with a class using `==`/`!=`; it is not covered by the general implicit-conversion relation.

ANSWER
======
Keep as is
### Operators and casts

#### CASE 23 — Unary numeric, logical, and increment operators

SOURCE
======
```cpp
-x; +x; !x; ++x; x--;
```

AST
===
```text
ProgramAst.body
└── ExpressionStatementAst for each UnaryExpr:
    op="-"|"+"|"!"|"++"|"--", operand=VariableExpr("x"),
    postfix=false,false,false,false,true
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which operand domains/results apply, and which forms require a mutable value?

CURRENT BEHAVIOR: Unary `+/-` accept numeric (`bool/char/int/float`) and preserve type; `!` accepts any known value and returns bool; `++/--` require mutable `char/int/float` and preserve type. Function values reject these operators.

ANSWER
======
Keep as is
#### CASE 24 — Arithmetic and list concatenation

SOURCE
======
```cpp
a + b; a - b; a * b; a / b; a % b; left + right;
```

AST
===
```text
ProgramAst.body
└── ExpressionStatementAst nodes containing BinaryExpr(op="+"|"-"|"*"|"/"|"%",
    left=VariableExpr(...),right=VariableExpr(...)); final `+` has list operands
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which numeric promotions and overloads apply, and where are implicit conversions inserted?

CURRENT BEHAVIOR: Numeric operations yield float if either side is float, otherwise int; `%` rejects float. Same-typed lists support `+`. Other mixtures fail.

ANSWER
======
Keep as is
#### CASE 25 — Comparisons and equality

SOURCE
======
```cpp
a < b; a == b; first != second;
```

AST
===
```text
ProgramAst.body
└── BinaryExpr nodes with op="<"|"=="|"!=", left/right=VariableExpr(...),
    inferredType=<to derive>,mutableValue=false,explicitCast=false
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which pairs are comparable, are equality and ordering different, and must composite types exactly match?

CURRENT BEHAVIOR: Numeric types compare across numeric types. Lists/pairs/sets/maps require equal types and recursively comparable components. Custom values require equal custom types; structs/classes only support `==`/`!=`. Same-typed functions support only equality/inequality.

ANSWER
======
Keep as is but this may open to change
#### CASE 26 — Logical operators and conditions

SOURCE
======
```cpp
left && right; left || right;
```

AST
===
```text
ProgramAst.body
└── BinaryExpr(op="&&"|"||",left=VariableExpr("left"),right=VariableExpr("right"))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Must operands be bool or merely truth-convertible, and is short-circuit behavior semantic?

CURRENT BEHAVIOR: Any known value type is accepted and the result is bool; generated C++ preserves short-circuiting. Function values are rejected before this rule.

ANSWER
======
Keep as is
#### CASE 27 — Bitwise and shift operators

SOURCE
======
```cpp
a & b; a | b; a ^ b; a << b; a >> b;
```

AST
===
```text
ProgramAst.body
└── BinaryExpr nodes with corresponding `op`, left=VariableExpr("a"),right=VariableExpr("b")
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which integral-like types and promotions apply; what is the result type and shift validity policy?

CURRENT BEHAVIOR: Both operands must be `bool`, `char`, or `int`; result is `int`. No separate static shift-count range rule is present.

ANSWER
======
Keep mostly as is but return type is defaulted to whatever the left type is. 
Though since all three types freely convert, this shoyld not be too weird.
#### CASE 28 — Membership

SOURCE
======
```cpp
x in values; key in counts; i in range(5);
```

AST
===
```text
ProgramAst.body
└── three BinaryExpr(op="in",left=VariableExpr(...),
    right=VariableExpr("values"|"counts") or CallExpr("range",args=[Int("5")]))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
What member type is tested for each iterable, are sublists special, and which conversions are allowed?

CURRENT BEHAVIOR: Right side must be collection or range. Range requires int; map/set use their first subtype; list uses its element type and has a sublist special case. Result is bool.

ANSWER
======
Keep
#### CASE 29 — Scalar explicit casts

SOURCE
======
```cpp
int(text); float(i); string(value); bool(items);
```

AST
===
```text
ProgramAst.body
└── CastExpr nodes with targetType=Type(Int|Float|List<Char>|Bool),
    operand=VariableExpr(...), explicitCast=true
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which source/target pairs are legal, may casts affect later compatibility, and which failures are static versus runtime?

CURRENT BEHAVIOR: Scalar-to-scalar casts are allowed; scalar-to-string and string-to-scalar are allowed. `void` casts are rejected. Conversion helpers may perform runtime parsing/checks.

ANSWER
======
Keep
#### CASE 30 — Collection and comparator casts

SOURCE
======
```cpp
Set(values); List(seen); Map(entries);
Heap<int> heap(greater);
```

AST
===
```text
ProgramAst.body
├── CastExpr(targetType=Type(Set,subtypes=[]),operand=VariableExpr("values"))
├── CastExpr(targetType=Type(List,subtypes=[]),operand=VariableExpr("seen"))
├── CastExpr(targetType=Type(Map,subtypes=[]),operand=VariableExpr("entries"))
└── VariableDeclarationAst(type=TypeSyntax("Heap",arguments=[TypeSyntax("int")]),
    names=["heap"],initializers=[VariableExpr("greater")],continuationTokenIndex=<(>) )
```

SEMANTIC INFORMATION / QUESTIONS
===============================
When may omitted type arguments be inferred from a cast operand, and how is a typed Heap comparator initializer distinguished from such a cast?

CURRENT BEHAVIOR: The analyzer resolves shorthand conversions among supported List/Set/Map/Range/linear/Heap shapes. A typed Heap declaration may take a comparator initializer, which must have `bool(T,T)` shape; that declaration path is separate from `CastExpr`.

ANSWER
======
when List has () it is default size, the otehrs have the Bool(T,T)cmp function. Basically keep as is. If type info is wrong for anything not just this, give the expected correct type.
### Calls, access, indexing, and literals

#### CASE 31 — Direct function call

SOURCE
======
```cpp
add(1, 2);
```

AST
===
```text
ProgramAst.body
└── ExpressionStatementAst
    └── CallExpr(callee="add",receiver=null,arguments=[Int("1"),Int("2")],
        argumentNames=["",""],functionType=Unknown,partialApplication=false)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
How is the callee resolved; what count, conversion, copy/mutability, and return rules apply?

CURRENT BEHAVIOR: A declared signature is checked left-to-right. Too many arguments fail; a complete argument list returns the declared return type.

ANSWER
======
Keep as is. Though if multiple errors arise in a function usage, return all errors.
Also tell the correct type.
#### CASE 32 — Function variable call and partial application

SOURCE
======
```cpp
int(int, int) op = add;
int(int) addFive = op(5);
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst(type=TypeSyntax(return int,functionParameters=[int,int],functionType=true),
│   names=["op"],initializers=[VariableExpr("add")])
└── VariableDeclarationAst(type=TypeSyntax(return int,functionParameters=[int],functionType=true),
    names=["addFive"],initializers=[CallExpr(callee="op",receiver=null,args=[Int("5")],
    functionType=<derive>,partialApplication=<derive>)])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Does a proper prefix always create a partial application, and how are remaining copy modes represented?

CURRENT BEHAVIOR: Fewer-than-required arguments produce a function type for remaining parameters and set `partialApplication=true`; too many fail.

ANSWER
======
This should be kept the same, though errors should refer to correct type.
#### CASE 33 — Non-copy collection parameter mutability

SOURCE
======
```cpp
void mutate(List<int> xs) { xs.add(1); }
mutate([1, 2]);
```

AST
===
```text
ProgramAst.body
├── FunctionDeclarationAst(returnType=void,name="mutate",
│   parameters=[ParameterSyntax(type=List<int>,name="xs",copyParameter=false)],body=[...])
└── ExpressionStatementAst(CallExpr("mutate",receiver=null,
    arguments=[ListLiteralExpr(elements=[Int("1"),Int("2")])]))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which parameter modes require mutable arguments, and do strings/classes follow collection rules?

CURRENT BEHAVIOR: Non-copy string/collection parameters require mutable argument expressions. `copy` allows temporaries and deep-copies; copy eligibility is collection, string, or class.

ANSWER
======
This is fine but be explicit when copy fails same with copy() the function.
#### CASE 34 — Recursion and call-before-declaration

SOURCE
======
```cpp
int f(int n) { if (n) { return f(n - 1); } return 0; }
int x = later();
int later() { return 1; }
```

AST
===
```text
ProgramAst.body
├── FunctionDeclarationAst(name="f",parameters=[int n],body=[
│   IfStatementAst(condition=VariableExpr("n"),thenBody=[
│     ReturnStatementAst(CallExpr("f",args=[BinaryExpr("-",VariableExpr("n"),Int("1"))]))]),
│   ReturnStatementAst(Int("0"))])
├── VariableDeclarationAst(type=int,names=["x"],initializers=[CallExpr("later",args=[])])
└── FunctionDeclarationAst(name="later",parameters=[],body=[ReturnStatementAst(Int("1"))])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
When is a function signature registered, are forward calls legal, and are mutually recursive functions supported?

CURRENT BEHAVIOR: A top-level function is inserted before its body, enabling direct recursion. Later functions have not yet been registered, so forward calls fail.

ANSWER
======
This is fine but like with varoiable clearly distinguish between undeclared function in scope and forward decleration.
#### CASE 35 — User method call

SOURCE
======
```cpp
counter.increment();
```

AST
===
```text
ProgramAst.body
└── ExpressionStatementAst
    └── CallExpr(callee="increment",receiver=VariableExpr("counter"),arguments=[],
        argumentNames=[],functionType=Unknown,partialApplication=false)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
How is receiver type resolved, which method table is searched, and is a mutable receiver required?

CURRENT BEHAVIOR: Struct/class method lookup uses the receiver custom type, then checks exact argument count and conversions. The general method path does not separately require receiver mutability.

ANSWER
======
This is fine.
#### CASE 36 — Function-valued aggregate field call

SOURCE
======
```cpp
calculator.operation(1, 2);
```

AST
===
```text
ProgramAst.body
└── ExpressionStatementAst(CallExpr(callee="operation",receiver=VariableExpr("calculator"),
    arguments=[Int("1"),Int("2")],functionType=Unknown,partialApplication=false))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Does a function-valued field take precedence over a method of the same name, and may it partially apply?

CURRENT BEHAVIOR: Function fields are checked before declared methods and currently require exactly all parameters; this path does not partial-apply.

ANSWER
======
This is fine
#### CASE 37 — Field access

SOURCE
======
```cpp
point.x;
```

AST
===
```text
ProgramAst.body
└── ExpressionStatementAst
    └── FieldExpr(base=VariableExpr(name="point"),field="x",
        inferredType=<derive>,mutableValue=<propagate>)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Must the base be a custom type, how are missing fields diagnosed, and is visibility always public?

CURRENT BEHAVIOR: Base must be struct/class; fields are public and looked up by name with suggestions. Field mutability equals base mutability.

ANSWER
======
Yes, though explicitly use Levenstien if type name is illegal.
#### CASE 38 — List and map indexing

SOURCE
======
```cpp
values[i]; counts[key];
```

AST
===
```text
ProgramAst.body
├── IndexExpr(base=VariableExpr("values"),index=VariableExpr("i"))
└── IndexExpr(base=VariableExpr("counts"),index=VariableExpr("key"))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which receivers are indexable, what index conversion is permitted, and does lookup produce an lvalue?

CURRENT BEHAVIOR: List index must be exactly int; map key may be implicitly convertible unless explicitly cast. Result is element/value type and inherits base mutability.

ANSWER
======
This is good.
#### CASE 39 — Pair indexing

SOURCE
======
```cpp
pair[0]; pair[1]; pair[i];
```

AST
===
```text
ProgramAst.body
└── IndexExpr nodes(base=VariableExpr("pair"), index=LiteralExpr(Int,"0") |
    LiteralExpr(Int,"1") | VariableExpr("i"))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Must the index be a compile-time literal, and can heterogeneous component access have a dynamic type?

CURRENT BEHAVIOR: Only literal `0` or `1` is accepted; it selects the corresponding subtype and inherits base mutability.

ANSWER
======
I dont know when you would want pair[i] but I think this shpould exist. This should return RTE if i != 0 or i != 1
#### CASE 40 — Slice bounds and result

SOURCE
======
```cpp
values[left:right];
```

AST
===
```text
ProgramAst.body
└── ExpressionStatementAst
    └── SliceExpr(base=VariableExpr("values"),start=VariableExpr("left"),
        end=VariableExpr("right"))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which types are sliceable, how are missing bounds represented/typed, and is the result mutable?

CURRENT BEHAVIOR: Only `List<T>` (including string) is sliceable; current syntax and analysis require both bounds and both must be int. Result is the same list type and is not marked mutable. Runtime supports negative/clamped bounds.

ANSWER
======
Change this. 
lst[:x] = list[0:x] and lst[a:] = lst[a:len(lst)] otherwise this is perefct, 
also this leads to lst1 = lst[:] being a copy exactly like POython
#### CASE 41 — List and set literal inference

SOURCE
======
```cpp
[1, 2.0]; {1, 2};
```

AST
===
```text
ProgramAst.body
├── ListLiteralExpr(elements=[LiteralExpr(Int,"1"),LiteralExpr(Float,"2.0")])
└── SetLiteralExpr(elements=[LiteralExpr(Int,"1"),LiteralExpr(Int,"2")])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
How is the common element type chosen; is inference directional; must set elements be orderable/hashable?

CURRENT BEHAVIOR: The first element fixes the inferred element type; later elements must implicitly convert to it. Empty literals fail in general expression analysis, though typed declaration emitters handle contextual empty literals.

ANSWER
======
Yeah but types beat first element inference. 
#### CASE 42 — Map and pair literals

SOURCE
======
```cpp
{1:"one", 2:"two"}; (1, 2.0);
```

AST
===
```text
ProgramAst.body
├── MapLiteralExpr(entries=[MapLiteralEntry(key=Int("1"),value=String("one")),
│                           MapLiteralEntry(key=Int("2"),value=String("two"))])
└── PairLiteralExpr(first=LiteralExpr(Int,"1"),second=LiteralExpr(Float,"2.0"))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
How are key/value types unified; which key types are legal; does a pair use context or component inference?

CURRENT BEHAVIOR: First map entry fixes key/value types and later entries convert to them. Pair type is constructed directly from its two inferred component types.

ANSWER
======
See 41
#### CASE 43 — Empty and nested collection literals

SOURCE
======
```cpp
List<List<int>> grid = [[], [1]];
var empty = [];
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst(type=List<List<int>>,names=["grid"],
│   initializers=[ListLiteralExpr(elements=[ListLiteralExpr([]),ListLiteralExpr([Int("1")])])])
└── VariableDeclarationAst(inferredType=true,names=["empty"],
    initializers=[ListLiteralExpr(elements=[])])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
How far does expected-type context flow into nested empty literals, and which empty forms are inferable?

CURRENT BEHAVIOR: Typed declaration-specific literal emitters provide recursive context. Standalone or `var` empty list/set/map literals cannot infer a type.

ANSWER
======
This is good.
### Statements and control flow

#### CASE 44 — Expression statements

SOURCE
======
```cpp
f(); x++; 1 + 2;
```

AST
===
```text
ProgramAst.body
├── ExpressionStatementAst(CallExpr("f",receiver=null,args=[]))
├── ExpressionStatementAst(UnaryExpr(op="++",operand=VariableExpr("x"),postfix=true))
└── ExpressionStatementAst(BinaryExpr(op="+",left=Int("1"),right=Int("2")))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Are only effectful expressions legal, and are print/list operations special statements or ordinary calls?

CURRENT BEHAVIOR: Specialized list/print/describe paths run first; increment/decrement is accepted; otherwise a successfully analyzed expression is emitted, including pure values.

ANSWER
======
This is fine. Keep what you have now.
#### CASE 45 — Return forms and conversion

SOURCE
======
```cpp
int f() { return 1; }
void g() { return; }
```

AST
===
```text
ProgramAst.body
├── FunctionDeclarationAst(returnType=int,name="f",parameters=[],body=[
│   ReturnStatementAst(value=LiteralExpr(Int,"1"),valueTokens=[1],valueOffset=7)])
└── FunctionDeclarationAst(returnType=void,name="g",parameters=[],body=[
    ReturnStatementAst(value=null,valueTokens=[],valueOffset=0)])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Must value presence match return type, what conversion applies, and is all-path return required?

CURRENT BEHAVIOR: Presence must match void/non-void and values must implicitly convert. No all-path return analysis is performed by the CP++ checker.

ANSWER
======
Yeah plz add all-path return analysis to the static analysis step.
#### CASE 46 — Break and continue targeting

SOURCE
======
```cpp
while (true) { for (var x in values) { break; } continue; }
```

AST
===
```text
ProgramAst.body
└── WhileStatementAst(condition=Bool("true"),body=[
    ForEachStatementAst(inferredVariable=true,variableName="x",iterable=VariableExpr("values"),
      body=[SimpleControlStatementAst(kind=Break)]),
    SimpleControlStatementAst(kind=Continue)],nobreakBranch=null)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which loop is targeted and how does a nested break affect each `nobreak` branch?

CURRENT BEHAVIOR: Both target the nearest enclosing loop. `break` clears that loop's completion flag; `continue` does not.

ANSWER
======
This is perfect.
#### CASE 47 — If/else-if/else

SOURCE
======
```cpp
if (a) { int x = 1; } else if (b) { int y = 2; } else { int z = 3; }
```

AST
===
```text
ProgramAst.body
└── IfStatementAst(condition=VariableExpr("a"),conditionTokens=[a],conditionOffset=<a>,
    thenBody=BlockAst([decl x]),
    elseIfBranches=[ConditionalBranchAst(headerSyntax=<else if>,condition=VariableExpr("b"),
      conditionTokens=[b],conditionOffset=<b>,syntaxOk=true,body=BlockAst([decl y]))],
    elseBranch=CompletionBranchAst(headerSyntax=<else>,body=BlockAst([decl z])))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
What converts to bool; do branches have independent scopes; should flow merge types/definite assignment?

CURRENT BEHAVIOR: Conditions must implicitly convert to bool. Each owned block removes its declarations afterward; no branch-flow merge or definite-assignment analysis exists.

ANSWER
======
This is fine.
#### CASE 48 — While with `nobreak`

SOURCE
======
```cpp
while (condition) { if (stop) { break; } } nobreak { print("done"); }
```

AST
===
```text
ProgramAst.body
└── WhileStatementAst(condition=VariableExpr("condition"),body=BlockAst([
    IfStatementAst(condition=VariableExpr("stop"),thenBody=[Break])]),
    nobreakBranch=CompletionBranchAst(headerSyntax=<nobreak>,
      body=BlockAst([ExpressionStatementAst(CallExpr("print",args=[String("done")]))])))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Precisely when does `nobreak` run, and what scopes/context does it inherit?

CURRENT BEHAVIOR: A per-loop flag starts true and direct/nested break targeting this loop makes it false; `nobreak` executes only if the flag remains true.

ANSWER
======
This is great
#### CASE 49 — Rep count

SOURCE
======
```cpp
rep n { work(); } nobreak { done(); }
```

AST
===
```text
ProgramAst.body
└── RepStatementAst(count=VariableExpr("n"),countTokens=[n],countOffset=<n>,
    body=BlockAst([CallExpr statement "work"]),
    nobreakBranch=CompletionBranchAst(body=BlockAst([CallExpr statement "done"])))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which numeric types/count values are legal, when is count evaluated, and what happens for negatives/fractions?

CURRENT BEHAVIOR: Bool/char/int/float counts are accepted and cast to int; the readable non-submit loop evaluates the limit once. Negative counts naturally execute zero iterations.

ANSWER
======
ACtually fix this, if negative give a RTE, 0 is fine. Note: this only applies to rep().
for in range, should not compllain if range is malformed. Defi not std for loops.
#### CASE 50 — Traditional for clause kinds and scope

SOURCE
======
```cpp
for (int i = 0; i < n; i++) { use(i); } nobreak { done(); }
```

AST
===
```text
ProgramAst.body
└── ForStatementAst
    ├── initializer: ForClauseAst(kind=VariableDeclaration,type=int,inferredType=false,
    │   names=["i"],expressions=[Int("0")],tokens=<clause>,offset=<init>)
    ├── condition=BinaryExpr("<",VariableExpr("i"),VariableExpr("n")); conditionTokens/offset=<source>
    ├── iteration: ForClauseAst(kind=Expression,expressions=[UnaryExpr("++",VariableExpr("i"),postfix=true)],tokens=<clause>)
    ├── body=BlockAst([CallExpr statement "use"])
    └── nobreakBranch=CompletionBranchAst(body=[CallExpr statement "done"])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
How are `Empty`, declaration, assignment, expression, and error clauses validated; where does initializer scope end?

CURRENT BEHAVIOR: Initializer may declare; iteration may not. Condition defaults true and otherwise bool-converts. New initializer names remain visible through condition/iteration/body and are erased after the loop before `nobreak` lowering.

ANSWER
======
Keep
#### CASE 51 — Foreach element typing

SOURCE
======
```cpp
for (var item in source) { use(item); }
for (Pair<int,string> entry in counts) { use(entry); }
```

AST
===
```text
ProgramAst.body
├── ForEachStatementAst(variableType=TypeSyntax(),variableName="item",inferredVariable=true,
│   iterable=VariableExpr("source"),iterableTokens=[source],variableOffset/iterableOffset=<source>,body=[...])
└── ForEachStatementAst(variableType=TypeSyntax("Pair",[int,string]),variableName="entry",
    inferredVariable=false,iterable=VariableExpr("counts"),body=[...])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which values are iterable, what element type is produced, is explicit loop-variable conversion allowed, and is it an alias/copy?

CURRENT BEHAVIOR: List/Set yield subtype, Map yields `Pair<K,V>`, Range yields int. Stack/Queue/Deque/Heap are not foreach iterables. Explicit type must accept implicit conversion. The loop variable is mutable and scoped to the body.

ANSWER
======
Keep. Though in st/heap/queue/dequeu case suggest in list(var) as a hint.
### Functions and aggregates

#### CASE 52 — Function signature and parameter scope

SOURCE
======
```cpp
float average(int total, int count) { return total / count; }
```

AST
===
```text
ProgramAst.body
└── FunctionDeclarationAst(returnType=TypeSyntax("float"),name="average",nameSpan=<average>,
    parameters=[ParameterSyntax(type=int,name="total",copyParameter=false),
                ParameterSyntax(type=int,name="count",copyParameter=false)],
    body=BlockAst([ReturnStatementAst(BinaryExpr("/",VariableExpr("total"),VariableExpr("count")))],
                  hasClosingSyntax=true))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which declaration scopes permit functions; must parameter names be unique; are outer variables visible; how is return conversion checked?

CURRENT BEHAVIOR: Top-level functions have a separate local variable map containing only parameters, so top-level variables are not captured. Return int converts to float. Duplicate parameter handling is not an explicit distinct diagnostic and map insertion can overwrite.

ANSWER
======
This is good, just be explicit and give hints.
#### CASE 53 — Parameter modifier rules

SOURCE
======
```cpp
void f(copy List<int> xs, deep List<int> old, copy int bad) {}
```

AST
===
```text
FunctionDeclarationAst(returnType=void,name="f",parameters=[
 ParameterSyntax(type=List<int>,name="xs",copyParameter=true,modifier="copy",modifierSpan=<copy>),
 ParameterSyntax(type=List<int>,name="old",copyParameter=true,modifier="deep",modifierSpan=<deep>),
 ParameterSyntax(type=int,name="bad",copyParameter=true,modifier="copy",modifierSpan=<copy>)],body=[])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Is `deep` an alias or removed syntax; which types are copy-eligible; are modes part of overload/name identity?

CURRENT BEHAVIOR: Declaration parameters reject `deep` with replacement help. `copy` is allowed only for collection, string, or class. Functions do not overload; duplicate name alone fails.

ANSWER
======
Good
#### CASE 54 — Nested function declaration

SOURCE
======
```cpp
void outer() { int inner() { return 1; } }
```

AST
===
```text
ProgramAst.body
└── FunctionDeclarationAst(name="outer",body=BlockAst([
    FunctionDeclarationAst(returnType=int,name="inner",parameters=[],
      body=BlockAst([ReturnStatementAst(Int("1"))]))]))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
If structurally parsed, are nested declarations legal, do they capture, and what namespace/lifetime do they have?

CURRENT BEHAVIOR: Function lowering uses global function state/output and is designed for top-level declarations; nested-function semantics are not cleanly established and should be decided explicitly.

ANSWER
======
I dont see the issue here. 
#### CASE 55 — Struct declaration and fields

SOURCE
======
```cpp
struct Point { int x; int y; }
```

AST
===
```text
ProgramAst.body
└── AggregateDeclarationAst(name="Point",nameSpan=<Point>,isClass=false,
    body=BlockAst(statements=[
      VariableDeclarationAst(type=int,names=["x"],initializers=[]),
      VariableDeclarationAst(type=int,names=["y"],initializers=[])],hasClosingSyntax=true))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which members are legal; must fields be typed; can fields duplicate or contain custom types; what copy/equality/print properties are guaranteed?

CURRENT BEHAVIOR: Bodies allow typed fields, methods, comments. Duplicate fields fail. Inline struct fields cannot contain custom types. Fields are public; generated structs have value semantics, equality, printing, and deep-copy support.

ANSWER
======
Good, but rmemeber the changes in struct analysis
#### CASE 56 — Class declaration and default nullability

SOURCE
======
```cpp
class Node { int value; Node next; }
Node empty;
```

AST
===
```text
ProgramAst.body
├── AggregateDeclarationAst(name="Node",isClass=true,body=[
│   VariableDeclarationAst(type=int,names=["value"]),
│   VariableDeclarationAst(type=Node,names=["next"])])
└── VariableDeclarationAst(type=Node,names=["empty"],initializers=[])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Are all class values nullable references, how do assignment/copy differ, and may class fields be recursive?

CURRENT BEHAVIOR: Class values use smart-pointer/reference semantics and default to `NULL`; assignment aliases. Recursive/custom fields are allowed. `copy()` produces an independent deep copy.

ANSWER
======
Good
#### CASE 57 — Aggregate member validity and collisions

SOURCE
======
```cpp
struct S { int x; int x; void x() {} if (true) {} }
```

AST
===
```text
AggregateDeclarationAst(name="S",isClass=false,body=BlockAst([
 VariableDeclarationAst(type=int,names=["x"]),
 VariableDeclarationAst(type=int,names=["x"]),
 FunctionDeclarationAst(returnType=void,name="x",parameters=[],body=[]),
 IfStatementAst(condition=Bool("true"),thenBody=[]) ]))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Do fields and methods share a namespace, and should invalid member statements still have children analyzed?

CURRENT BEHAVIOR: Fields and methods use separate maps, so a field and method may share a name; duplicates within each map fail. Other body statements report that struct bodies require typed fields (wording also used for classes).

ANSWER
======
Fine, give hints in failure cases and references to what other variable/function caused the collision, also a varaible and function (in general) can not share the same name anymore.
#### CASE 58 — Method scope and implicit fields

SOURCE
======
```cpp
class Counter { int x; void add(int n) { x += n; } }
```

AST
===
```text
AggregateDeclarationAst(name="Counter",isClass=true,body=[
 VariableDeclarationAst(type=int,names=["x"]),
 FunctionDeclarationAst(returnType=void,name="add",parameters=[ParameterSyntax(type=int,name="n")],
   body=[AssignmentStatementAst(operation="+=",targets=[VariableExpr("x")],values=[VariableExpr("n")])])])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Are fields implicit names in method scope; what happens on parameter/field collision; can methods call later methods?

CURRENT BEHAVIOR: Method checking seeds the variable map with fields, then parameters overwrite same-name fields. A method is registered before its body, but later sibling methods are not yet present.

ANSWER
======
Methods should stored identical to class variables within the class, also I am sad to say this, but allow self as a reference to the class, its uiseful even if a bit OOPish.
#### CASE 59 — Aggregate constructor call

SOURCE
======
```cpp
Point p = Point(1, 2);
Node n = Node(1, NULL);
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst(type=Point,names=["p"],initializers=[
│   CallExpr(callee="Point",receiver=null,args=[Int("1"),Int("2")])])
└── VariableDeclarationAst(type=Node,names=["n"],initializers=[
    CallExpr(callee="Node",receiver=null,args=[Int("1"),Null("NULL")])])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Does constructor order equal field declaration order; must every field be supplied; what conversions and null rules apply?

CURRENT BEHAVIOR: Constructor calls require exactly one argument per field in declaration order. Each converts to its field type; `NULL` is specially accepted for class fields. Both struct and class construction use `CallExpr`.

ANSWER
======
Nice
#### CASE 60 — Aggregate/type-name collisions

SOURCE
======
```cpp
struct int {}
class Point {}
struct Point {}
```

AST
===
```text
ProgramAst.body
├── AggregateDeclarationAst(name="int",isClass=false,body=[])
├── AggregateDeclarationAst(name="Point",isClass=true,body=[])
└── AggregateDeclarationAst(name="Point",isClass=false,body=[])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which built-in/custom names share the type namespace, and may type names collide with values/functions?

CURRENT BEHAVIOR: Aggregate names cannot duplicate a built-in or prior aggregate type. Type, variable, and function stores are otherwise separate, so cross-category collisions are not uniformly rejected.

ANSWER
======
A struct and class cannot share the same name.
### Builtins, containers, input, and print

#### CASE 61 — `len` and `copy`

SOURCE
======
```cpp
len(values); copy(value);
```

AST
===
```text
ProgramAst.body
├── CallExpr(callee="len",receiver=null,args=[VariableExpr("values")])
└── CallExpr(callee="copy",receiver=null,args=[VariableExpr("value")])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which values have length; which values can be copied; are these first-class functions or special calls?

CURRENT BEHAVIOR: `len` takes one collection (string counts as list) and returns int; Range/Pair are not accepted. `copy` takes one non-unknown/non-void value and returns the same type. Only some numeric builtins have first-class `builtinFunctionType` entries; `len`/`copy` do not.

ANSWER
======
okay fine
#### CASE 62 — Numeric and range builtins

SOURCE
======
```cpp
range(1, 10, 2); min(values); max(a, b); sum(values); abs(x);
```

AST
===
```text
ProgramAst.body
└── CallExpr nodes(callee="range"|"min"|"max"|"sum"|"abs",receiver=null,
    arguments=<shown positional expressions>,argumentNames=all empty)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
What arities/types/results apply; are empty collections checked statically/runtime; may these builtins be stored as function values?

CURRENT BEHAVIOR: `range` takes 1–3 explicit/int arguments and returns Range. `min/max` accept one List/Set/Map or multiple exactly same-typed values. `sum` takes a numeric-element List and returns float only for float elements, otherwise int. `abs` takes int/float and preserves type. `sum/min/max/abs` have limited hard-coded first-class signatures.

ANSWER
======
This is good enough
#### CASE 63 — Target-typed scalar and pair input

SOURCE
======
```cpp
int x = input();
Pair<int, char> p = input();
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst(type=int,names=["x"],initializers=[CallExpr("input",args=[])])
└── VariableDeclarationAst(type=Pair<int,char>,names=["p"],initializers=[CallExpr("input",args=[])])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
How does expected target type reach `input`, and can bare/nested input have an independent type?

CURRENT BEHAVIOR: Declaration/assignment token adapters detect `input(...)` as the entire RHS and emit by target type. Bare expression analysis rejects `input` except through that contextual path. Scalars, strings, pairs, and supported lists have input emitters.

ANSWER
======
Keep
#### CASE 64 — Shaped list input and multi-target input

SOURCE
======
```cpp
List<List<int>> grid = input(rows, cols);
a, b = input();
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst(type=List<List<int>>,names=["grid"],
│   initializers=[CallExpr(callee="input",args=[VariableExpr("rows"),VariableExpr("cols")])])
└── AssignmentStatementAst(operation="=",targets=[VariableExpr("a"),VariableExpr("b")],
    values=[CallExpr(callee="input",args=[])],target/value token slices and offsets=<source>)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
How many size arguments correspond to list depth; what types/ranges are valid; how is one input call distributed across targets?

CURRENT BEHAVIOR: Size expressions must be int; list input supports dimensional sizes and one-dimensional unsized input, with runtime nonnegative checks. One entire-RHS input is repeated using each target's type. `var` cannot supply context.

ANSWER
======
Keep
#### CASE 65 — Print values and named options

SOURCE
======
```cpp
print(values, name, end = "", delim = ",");
```

AST
===
```text
ProgramAst.body
└── ExpressionStatementAst
    └── CallExpr(callee="print",receiver=null,
        arguments=[VariableExpr("values"),VariableExpr("name"),String(""),String(",")],
        argumentNames=["","","end","delim"])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which types are printable, where may named options appear, and what option types/collection interactions apply?

CURRENT BEHAVIOR: A specialized statement emitter validates `end` (string/char/`flush`) and `delim` (string/char), option ordering/duplication, and requires a List-like delimited argument where applicable. Composite/custom types have generated print helpers.

ANSWER
======
Keep but keep in mind print may get more declerations
#### CASE 66 — Describe

SOURCE
======
```cpp
describe(value);
```

AST
===
```text
ProgramAst.body
└── ExpressionStatementAst(CallExpr(callee="describe",receiver=null,
    arguments=[VariableExpr("value")],argumentNames=[""]))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Is `describe` a statement-only builtin, what argument forms are accepted, and what semantic type information does it expose?

CURRENT BEHAVIOR: It is recognized by the expression-statement lowering path and handled by a specialized print emitter, not the ordinary call analyzer.

ANSWER
======
Good enough, describe is not print.
#### CASE 67 — List mutation and ordering methods

SOURCE
======
```cpp
values.add(x); values.add(x, i); values.remove(i); values.sort(compare("field")); values.reverse();
```

AST
===
```text
ProgramAst.body
└── ExpressionStatementAst CallExpr nodes with receiver=VariableExpr("values"),
    callee="add"|"remove"|"sort"|"reverse",arguments=<source expressions/comparator calls>
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which methods mutate, what receiver lvalue is required, what element/index conversions apply, and which comparator forms are valid?

CURRENT BEHAVIOR: Several list statements are handled in `listsCppp.cpp` before general call analysis. Mutation requires suitable mutable storage; `add` supports value and optional index; sort supports named/default/custom/`compare(index|field)` comparator families with type-specific checks.

ANSWER
======
Keep this the same for now, but it will change VERY soon
#### CASE 68 — List searching/splitting

SOURCE
======
```cpp
values.find(x); values.find(sublist); values.split(delimiter);
```

AST
===
```text
ProgramAst.body
└── CallExpr nodes(receiver=VariableExpr("values"),callee="find"|"split",
    arguments=[VariableExpr("x"|"sublist"|"delimiter")])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
How do value versus sublist operands differ and what are result types?

CURRENT BEHAVIOR: `find` accepts an element or compatible same-list/sublist form and returns `List<int>`. `split` accepts one element or same-list delimiter and returns `List<List<T>>` (string follows List<char> behavior).

ANSWER
======
See 67
#### CASE 69 — Stack, queue, deque, and heap methods

SOURCE
======
```cpp
stack.add(x); stack.top(); stack.pop(); deque.addFront(x); deque.popBack(); heap.push(x);
```

AST
===
```text
ProgramAst.body
└── CallExpr nodes(receiver=VariableExpr(container),callee=<shown method>,arguments=[]|[VariableExpr("x")])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which operation names belong to each container, which mutate, what is returned, and what empty-container behavior applies?

CURRENT BEHAVIOR: Stack/Queue use specialized statement `add` plus `top/pop`; Deque uses `addFront/addBack/front/back/popFront/popBack`; Heap uses specialized add/push compatibility plus `top/pop`. Argument types convert to element type. Access/removal returns element type; empty access is guarded by runtime helpers in readable mode.

ANSWER
======
Keep
#### CASE 70 — Set and map operations

SOURCE
======
```cpp
seen.add(x); seen.remove(x); counts.at(k); counts[k]; counts.prev(k); counts.hasNext(k);
```

AST
===
```text
ProgramAst.body
└── CallExpr/IndexExpr nodes with receiver/base=VariableExpr("seen"|"counts"),
    callee="add"|"remove"|"at"|"prev"|"hasNext", arguments=[VariableExpr(...)],
    or IndexExpr(index=VariableExpr("k"))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
What is key/member type, what mutates, what do removal and ordered-neighbor methods return, and is comparator capability semantic?

CURRENT BEHAVIOR: Set/Map add is specialized; remove requires mutable receiver and returns removed element/value. `at`/index returns map value. `prev/next` return key and `hasPrev/hasNext` return bool. Keys/values use implicit conversion rules; ordered containers carry comparator behavior.

ANSWER
======
Keep

#### CASE 71 — Null comparison and class assignment

SOURCE
======
```cpp
if (node == NULL) { node = Node(1, NULL); }
```

AST
===
```text
ProgramAst.body
└── IfStatementAst(condition=BinaryExpr(op="==",left=VariableExpr("node"),right=Null("NULL")),
    thenBody=[AssignmentStatementAst(operation="=",targets=[VariableExpr("node")],
      values=[CallExpr("Node",args=[Int("1"),Null("NULL")])])])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Is nullability limited to classes, does assignment accept null uniformly, and does dereference require static or runtime checking?

CURRENT BEHAVIOR: Only class types participate in `NULL ==/!=`; constructor arguments accept `NULL` for class-typed fields, and class values default to null. `NULL` is not generally implicitly convertible to a class for arbitrary assignment. Field/method access on null is represented as normal access and relies on runtime behavior/checking.

ANSWER
======
Anything should be comparable to NULL, it just is never == NULL.
### Cross-cutting semantic and recovery cases

#### CASE 72 — All implicit-conversion request sites

SOURCE
======
```cpp
float x = i; x = c; return i; f(i); if (items) {} for (float v in ints) {} [1, true];
```

AST
===
```text
ProgramAst.body contains, respectively:
VariableDeclarationAst(initializer=VariableExpr("i")); AssignmentStatementAst(value=VariableExpr("c"));
ReturnStatementAst(VariableExpr("i")); CallExpr("f",args=[VariableExpr("i")]);
IfStatementAst(condition=VariableExpr("items")); ForEachStatementAst(variableType=float,iterable=ints);
ListLiteralExpr(elements=[Int("1"),Bool("true")])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Should one conversion relation govern all sites, or do condition, operator, literal, foreach, parameter, return, declaration, and assignment contexts differ? Does explicit-cast provenance waive another conversion?

CURRENT BEHAVIOR: Most sites use `isImplicitlyConvertible`, but operators, literals, conditions, input, null, and specialized container emitters add exceptions. Scalars have asymmetric conversions; structured types generally require exact equality; collections/pairs convert to bool.

ANSWER
======
Fine
#### CASE 73 — Complete lvalue/mutability surface

SOURCE
======
```cpp
x = 1; obj.field++; values[i] += 2; make().field = 3; values.remove(); f(values);
```

AST
===
```text
ProgramAst.body contains assignment targets VariableExpr, FieldExpr, IndexExpr;
UnaryExpr("++",FieldExpr(...)); AssignmentStatementAst(target=FieldExpr(base=CallExpr("make"),...));
CallExpr(receiver=VariableExpr("values"),callee="remove"); CallExpr("f",args=[VariableExpr("values")])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Define assignable versus mutable, propagation through fields/indexing, temporary mutation, mutating receiver rules, and reference-like parameter requirements.

CURRENT BEHAVIOR: Variables are mutable; field/index mutability propagates from base; literals/calls/slices are not mutable. Assignment and `++/--` require mutable values. Some mutating methods enforce mutability in specialized paths; non-copy collection/string parameters require mutable arguments.

ANSWER
======
yes sure
#### CASE 74 — Namespace collision matrix

SOURCE
======
```cpp
int Thing;
int Thing() { return 1; }
struct Thing { int Thing; int Thing() { return Thing; } }
```

AST
===
```text
ProgramAst.body
├── VariableDeclarationAst(type=int,names=["Thing"])
├── FunctionDeclarationAst(returnType=int,name="Thing",parameters=[],body=[return 1])
└── AggregateDeclarationAst(name="Thing",isClass=false,body=[
    VariableDeclarationAst(type=int,names=["Thing"]),
    FunctionDeclarationAst(returnType=int,name="Thing",parameters=[],body=[return VariableExpr("Thing")])])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which of variables, parameters, functions, builtins, types, fields, and methods share namespaces; what shadows what; are constructor names reserved?

CURRENT BEHAVIOR: Variables, functions, custom types, fields, and methods are in separate containers. Visible-variable redeclaration is rejected. Function duplicates and type duplicates are rejected within their categories. Expression lookup prefers variables, while constructor/call routing adds contextual precedence.

ANSWER
======
Again group functions and variables in the same namespace, actually now that I think about it structs/classes are in the same symbol table, but struct methods live in the block of the struct, so 
Class A{
    g()
}
Class B{
    g()
}
is allowed.
#### CASE 75 — `ErrorExpr`

SOURCE
======
```cpp
int x = ;
```

AST
===
```text
ProgramAst.body
└── VariableDeclarationAst(type=int,names=["x"],
    initializers=[ErrorExpr(reason="expected expression",sourceColumn=<after =>,sourceSpan=<possibly invalid>)],
    syntaxOk=<statement recovery result>,syntaxError=<if owned by statement>)
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Should semantic analysis skip an error expression, assign `Unknown`, analyze siblings, or emit any new diagnostic?

CURRENT BEHAVIOR: Syntax parsing creates `ErrorExpr` for recovery; statement/token compatibility emitters generally own the established diagnostic. Invalid/unknown symbols are sometimes retained to suppress cascades.

ANSWER
======
This is ideal.
#### CASE 76 — `ErrorStatementAst` with recovered body

SOURCE
======
```cpp
mystery (x) { int y = missing; }
```

AST
===
```text
ProgramAst.body
└── ErrorStatementAst(kind=Error,reason=<parser recovery reason>,syntax=<header>,
    recoveredBody=BlockAst(statements=[VariableDeclarationAst(type=int,names=["y"],
      initializers=[VariableExpr("missing")])],hasClosingSyntax=true))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Should the recovered body be analyzed for independent errors, under which scope/context, and how are cascades capped?

CURRENT BEHAVIOR: Lowering reports the error statement's established reason and may preserve structural recovery; there is no standalone semantic-pass cascade policy yet.

ANSWER
======
Recovered bodies should be analyzed for independent semantic errors using only the lexical/function/loop/aggregate context that the parser successfully established. Failed semantic nodes should produce a poisoned/error state so downstream dependent checks do not emit cascading diagnostics. Independent siblings should still be analyzed.

#### CASE 77 — Per-node syntax recovery fields

SOURCE
======
```cpp
for (int i = 0 i < 3; i++) { print(i); }
```

AST
===
```text
ProgramAst.body
└── ForStatementAst(syntaxOk=false,syntaxErrorOffset=<header location>,
    syntaxError="for loop must use syntax for (init; condition; step)",
    initializer/condition/iteration=<best-effort ForClauseAst/Error data>,
    body=BlockAst([CallExpr statement "print"],hasClosingSyntax=true))
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Does syntax failure suppress all semantic checks for the node/body, or should independently sound subtrees be visited?

CURRENT BEHAVIOR: The for compiler reports the stored syntax error and returns without lowering the body. Other node kinds differ, especially around missing closing braces.

ANSWER
======
This is fine
#### CASE 78 — Diagnostic spans and compatibility tokens

SOURCE
======
```cpp
values[index] = input(size);
```

AST
===
```text
ProgramAst.body
└── AssignmentStatementAst(sourceSpan=<whole statement>,operation="=",operationSpan=<=>,
    targets=[IndexExpr(sourceSpan=<values[index]>,base=VariableExpr("values"),index=VariableExpr("index"))],
    values=[CallExpr(sourceSpan=<input(size)>,callee="input",args=[VariableExpr("size")])],
    targetTokens/valueTokens=<normalized slices>,targetOffsets=[0],valueOffsets=[16])
```

SEMANTIC INFORMATION / QUESTIONS
===============================
Which span owns each error, and may the semantic pass depend on token slices where AST structure exists? What semantic data must be added later versus derived in a side table?

CURRENT BEHAVIOR: Current declaration, assignment, input, print, and list paths still consume retained token slices/offsets for checking and precise diagnostics. Expression nodes carry spans plus semantic fields mutated in place.

ANSWER
======
Yeah for error, keep source spans in the tokenization phase and propogate it to oparsing ansd semantic analyis. I dont care what you do afterward since it is not STRICTLY necessary, but a good rule of thumb is just to keep it.
## Part 3 — Cross-Cutting Rule Tables

These tables name decisions; they intentionally do not choose future rules.

### Namespace dimensions

| Existing entity | Current store | Decisions required |
|---|---|---|
| Local/top-level variables and parameters | `declaredVariables` | Same-scope redeclaration, outer shadowing, use-before-declaration, parameter/field collision, capture. |
| Free functions | `declaredFunctions` | Forward visibility, recursion/mutual recursion, overloading, collision with values/types/builtins. |
| Structs and classes | `declaredStructs` + class-name set | Registration phase, cross-kind collision, incomplete/self reference, collision with functions/values. |
| Fields | per-type field map/order | Collision with methods, inherited visibility (none currently), implicit method-scope lookup. |
| Methods | per-type signature map | Declaration order, recursion/cross-calls, collision with fields/builtins. |
| Builtins | hard-coded call paths and a small function-type table | Reservation/shadowing, first-class availability, uniform call typing. |

### Implicit conversion sites

| Site | Inputs to the decision | Existing notable special cases |
|---|---|---|
| Declaration / assignment | source type, target type, explicit-cast flag, expected literal/input shape | Target-typed input, empty typed literals, class `NULL`, size initializers. |
| Return / function argument | expression type, declared type, parameter copy mode/value category | Non-copy collection/string arguments must be mutable. |
| Foreach variable | iterable element type, explicit variable type | Map yields Pair; Range yields int. |
| Condition / logical operator | expression operand types | Conditions use implicit-to-bool; logical operators accept any known values. |
| Binary operator | both operand types, operator, explicit-cast flags | Numeric promotion, exact composite equality, membership families. |
| Collection literal | expected type if any, first element/entry, later elements | General inference is first-entry directional; declaration emitters supply context. |

### Operator compatibility dimensions

| Family | Decisions required |
|---|---|
| Unary arithmetic / increment | Accepted numeric categories, result type, overflow behavior, lvalue requirement. |
| Arithmetic / modulo | Promotion lattice, list concatenation or other overloads, division/modulo zero policy. |
| Equality / ordering | Cross-numeric comparison, structural equality, class identity versus value equality, function equality. |
| Logical / truthiness | Whether operands must be bool or truth-convertible; short-circuit guarantees. |
| Bitwise / shifts | Accepted integral categories, promotions, negative/oversized shift rules. |
| Membership | Iterable families, member type, sublist semantics, conversion policy. |

### Scope and flow dimensions

| Construct | Scope/flow decisions |
|---|---|
| Program/block | Shadowing, declaration point, failed-symbol retention, name lifetime. |
| Branches | Independent scopes, flow-sensitive narrowing, definite assignment, merged state. |
| Loops | Header/body/nobreak scope, nearest-loop control, iteration variable alias/copy, completion facts. |
| Function/method | Outer capture, parameter scope, method implicit receiver/fields, all-path return, unreachable code. |
| Aggregate | Type registration phase, member declaration order, incomplete types. |

### Contextual typing and mutability dimensions

| Context | Questions to decide |
|---|---|
| Empty/nested literals | How expected types propagate and whether bidirectional inference is used. |
| `input` | Which enclosing nodes may provide target type; whether input ever has a standalone type. |
| `NULL` | Whether it has a nullable meta-type, which target types accept it, and how joins/comparisons work. |
| Lvalues | Variables, fields, list/map/pair indices, dereferenced classes, temporaries, slices. |
| Mutation | Assignment, compound assignment, increment, mutating method, alias parameter, foreach element. |
| Function values | Signature/copy-mode identity, partial application, callable fields, builtin functions. |

### Recovery dimensions

| Recovery input | Decisions required |
|---|---|
| `ErrorExpr` | Type/value-category placeholder, sibling traversal, duplicate diagnostic suppression. |
| `ErrorStatementAst` | Recovered-body traversal and synthetic context. |
| `syntaxOk=false` on structured nodes | Whether sound bodies/branches are still analyzed. |
| Unknown/failed symbols | Whether to insert poison symbols and how far poison propagates. |
| Missing close brace | Whether scope closes at EOF and which context errors remain meaningful. |

## Part 4 — Current Behavior

The following is a concise implementation snapshot, not a recommendation:

- Semantic checking is split among `AstLowerer`, `ExpressionAnalyzer`, declaration/assignment emitters, list/print/input helpers, and code-generation-specific branches.
- Name visibility is sequential. A single visible-variable map plus per-block erase lists rejects shadowing of any still-visible variable. Functions, types, fields, and methods live in separate maps.
- Custom type names are registered before aggregate bodies; function names are registered before their own bodies. This supports self-referential classes and direct recursion, but not general forward declarations.
- Types resolve to `PrimitiveType` plus subtypes/name. Function types store return type first in `subtypes`, followed by parameters, with copy-mode bits participating in equality.
- Scalar implicit conversion is permissive but asymmetric: bool/char/int convert among several scalar types, int converts to float, float implicitly converts only to bool. Structured types normally require exact equality; collections and pairs truth-convert to bool.
- Expression analysis mutates `Expr.inferredType`, `mutableValue`, `explicitCast`, `CallExpr.functionType`, and `CallExpr.partialApplication` while codegen later consumes them.
- Typed declaration emitters provide contextual behavior not available to general expression analysis, notably empty/nested literals, size initialization, class `NULL`, and target-typed input.
- Classes are nullable reference-like values and assignments alias; structs are inline values. Both use public fields, field-order constructors, generated equality and printing. Inline struct fields cannot contain custom types; class fields can.
- Conditions accept implicit conversion to bool. There is no general definite-assignment, all-path-return, unreachable-code, exhaustiveness, or data-flow analysis.
- `nobreak` is implemented with a per-loop completion flag changed by `break`; it is not a generic AST completion construct outside the associated loop forms.
- Readable versus submit mode changes generated runtime checks/helper detail, not the intended static semantic acceptance surface, although semantic and codegen logic are currently interleaved.
- Recovery behavior is node-specific. Some syntax errors stop traversal of the structured body; invalid symbols may be kept as `Unknown` to reduce cascades.

## Part 5 — Questions For Me

Before implementing a separate semantic pass, please decide:

1. Are top-level executable statements part of CP++, and are functions/aggregates allowed only at top level?
2. May an inner block shadow an outer variable? May parameters shadow fields or outer names?
3. Which entity categories share namespaces, and what are the collision/precedence rules for builtins, constructors, variables, functions, types, fields, and methods?
4. Are function/type declarations visible only after their declaration, throughout their scope, or after a predeclaration phase? Must mutual recursion work?
5. Should invalid declarations insert poison symbols, and how aggressively should semantic checking continue into invalid nodes and recovered bodies?
6. Confirm the legal type set, aliases, generic arities, `void` positions, function-type syntax, and whether function copy modes are type identity.
7. Confirm self-reference/incomplete-type rules separately for structs and classes.
8. Define declaration timing, default initialization, multi-declaration evaluation, and whether all inferred declarations require an initializer.
9. Define the implicit conversion matrix and whether it is uniform across declarations, assignments, returns, arguments, foreach, conditions, operators, and literal unification.
10. Define explicit-cast legality, contextual generic cast inference, and whether explicit-cast provenance may bypass later compatibility checks.
11. Define literal overflow/range rules, empty/nested literal contextual typing, and the common-type selection algorithm.
12. Define `NULL` as a semantic type/value: accepted targets, comparisons, default initialization, and dereference behavior.
13. Define every operator family's accepted types, promotions, result types, composite/custom overloads, and static versus runtime failure rules.
14. Define assignable versus mutable values and make mutating receiver/argument requirements uniform.
15. Confirm function partial application, first-class builtin coverage, callable-field behavior, and function equality semantics.
16. Confirm `copy` semantics and eligibility; decide whether `deep` is rejected everywhere or remains an alias in function-type syntax.
17. Confirm field/method collision rules, implicit field lookup in methods, method declaration order, and class versus struct method mutation semantics.
18. Confirm constructor arity/order/defaults/conversions and whether constructor syntax should stay represented as `CallExpr`.
19. Define condition truthiness and whether `&&`/`||` use exactly the same rule.
20. Define loop header/body/`nobreak` scopes, foreach alias/copy behavior, iterable families, and `rep` count conversion/evaluation.
21. Decide whether semantic analysis performs all-path return, unreachable-code, definite-assignment, or other flow checks in its first version.
22. Specify builtin families (`input`, `print`, `describe`, numeric/range, collection methods), especially which are expressions, statements, first-class functions, or context-sensitive forms.
23. Define semantic-pass diagnostic ownership: preferred source spans, one-error-per-poison policy, and which parser diagnostics must never be duplicated.
24. Decide whether the future semantic result mutates the current AST, uses side tables, or creates a typed AST—after the above behavior is fixed.
