# CP++

Early CP++ transpiler prototype.

## Build

```powershell
g++ .\cppp.cpp .\errors.cpp .\printCppp.cpp .\typesCppp.cpp -o .\build\Untitled-1.exe
```

## Usage

Generate C++:

```powershell
.\build\Untitled-1.exe --cppp .\in.cppp
```

Generate and compile:

```powershell
.\build\Untitled-1.exe --cppp .\in.cppp --compile
.\build\in.exe
```

## Current Features

- Top-level `print(...)` lowered into C++ `main`
- Multi-argument print with spaces
- `print(x, flush)` final-argument flush marker
- CP++ diagnostics with source lines
- Primitive declarations and initialized assignments for `bool`, `int`, `bigint`, `char`, `float`, and `bigfloat`
- Basic undeclared-variable checks for `print(...)`
