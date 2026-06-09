# CP++ Compiler

This project is a small compiler/transpiler for `.cppp` files. It reads a CP++ source file, emits a generated C++ file beside it, and can optionally compile that generated C++ into a Windows executable under `build/`.

## Project Layout

- `cppp.cpp` is the command-line driver. It validates arguments, reads the `.cppp` file, writes the generated `.cpp`, and optionally invokes `g++`.
- `tokenizer.cpp` / `tokenizer.h` scan source text into tokens with line and column spans.
- `typesCppp.cpp` / `typesCppp.h` translate CP++ declarations such as `int`, `bigint`, `char`, `float`, `bigfloat`, and `bool`.
- `printCppp.cpp` / `printCppp.h` translate `print(...)` statements to `cout`.
- `errors.cpp` / `errors.h` collect and print source-level diagnostics, including mapping some generated C++ compiler errors back to `.cppp` lines.
- `in.cppp` is the sample input file.

## Requirements

- `g++` with C++17 support
- GNU Make, such as MinGW Make, if you want to use the included `Makefile`

On MSYS2/MinGW for Windows, the Make command is often named `mingw32-make`. If `make` is not found, use `mingw32-make` in the commands below.

## Build the Compiler

```powershell
make
```

Or, on this Windows toolchain:

```powershell
mingw32-make
```

This builds the compiler executable:

```text
build/cppp.exe
```

You can also compile it manually:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic cppp.cpp errors.cpp printCppp.cpp tokenizer.cpp typesCppp.cpp -o build/cppp.exe
```

Create the `build` directory first if you compile manually.

## Compile `.cppp` Files

To only translate a CP++ file to C++:

```powershell
make transpile INPUT=in.cppp
```

This creates:

```text
in.cpp
```

To translate and compile the generated C++:

```powershell
make compile INPUT=in.cppp
```

This creates:

```text
in.cpp
build/in.exe
```

To compile and immediately run it:

```powershell
make run INPUT=in.cppp
```

The equivalent direct compiler command is:

```powershell
.\build\cppp.exe --cppp in.cppp --compile
.\build\in.exe
```

## CP++ Syntax Currently Supported

Every source statement must end with a semicolon.

Supported declarations:

```cpp
bool flag = true;
int count = 42;
bigint huge = 123456789012345678901234567890;
char letter = 'A';
float ratio = 3.14;
bigfloat precise = 3.1415926535;
```

Supported printing:

```cpp
print("count:", count);
print("done", flush);
```

`flush` is only valid as the final `print` argument.

## Cleaning Build Outputs

```powershell
make clean
```

This removes the `build` directory.
