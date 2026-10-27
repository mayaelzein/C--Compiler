# C-- Compiler

A compiler for C--, a simplified C-like language, implemented in C. The project includes lexical analysis, recursive-descent parsing, semantic checking, abstract syntax tree (AST) construction, three-address intermediate representation, and MIPS assembly code generation.

Developed as part of CSC coursework.

## Features

- **Lexical analysis:** Recognizes identifiers, integer constants, keywords, punctuation, and arithmetic, relational, and logical operators. Handles whitespace and block comments.
- **Recursive-descent parsing:** Parses function definitions, variable declarations, expressions, assignments, function calls, conditionals, loops, and return statements.
- **Semantic checking:** Uses scoped symbol tables to check declarations, identifier usage, function and variable usage, and function argument counts.
- **Abstract syntax trees:** Constructs AST nodes for expressions, statements, and functions, with an optional AST printer.
- **Intermediate representation:** Generates three-address instructions for computations, branches, function calls, and returns.
- **MIPS code generation:** Generates MIPS assembly for expressions, assignments, control flow, and function calls, including short-circuit evaluation of logical AND/OR.

## Compiler Pipeline

```text
C-- source code
      |
      v
Lexical scanner
      |
      v
Recursive-descent parser + semantic checks
      |
      v
Abstract syntax tree (AST)
      |
      v
Three-address intermediate code
      |
      v
MIPS assembly
```

## Build

Requires GCC and Make.

From the project directory, run:

```bash
make
```

The provided Makefile compiles the project using `gcc -Wall -g` and creates an executable named `compile`.

To remove the executable and object files:

```bash
make clean
```

## Usage

The compiler reads C-- source code from standard input and supports the following command-line options:

| Option | Purpose |
| --- | --- |
| `--chk_decl` | Enable declaration and semantic checks |
| `--print_ast` | Print the abstract syntax tree |
| `--gen_code` | Generate MIPS assembly |

Example commands:

```bash
./compile --chk_decl < examples/good.cmm
./compile --chk_decl --print_ast < examples/good.cmm
./compile --chk_decl --gen_code < examples/good.cmm > output.s
```

The final command redirects generated assembly into `output.s`.

These commands use a Unix-style shell environment.

## Example

The `examples/` directory contains a sample C-- program and its corresponding MIPS assembly.

- **`examples/good.cmm`** — Sample source program demonstrating recursion, conditional branching, and function calls.
- **`examples/good.s`** — MIPS assembly output illustrating function prologues and epilogues, conditional branches, recursive calls, and printing support.

## Project Structure

```text
C--Compiler/
├── examples/
│   ├── good.cmm
│   └── good.s
├── ast.c
├── ast.h
├── ast-print.c
├── codegen.c
├── codegen.h
├── driver.c
├── Makefile
├── parser.c
├── scanner.c
├── scanner.h
├── scanner-driver.c
├── symbol.h
├── threeaddr.h
└── README.md
```

### Main Source Files

| File | Description |
| --- | --- |
| `scanner.c`, `scanner.h` | Lexical scanning and token definitions |
| `parser.c` | Recursive-descent parsing and scope-aware semantic checks |
| `ast.c`, `ast.h` | AST construction, representation, and accessors |
| `ast-print.c` | AST printing utilities |
| `symbol.h` | Symbol table definitions |
| `threeaddr.h` | Three-address intermediate representation definitions |
| `codegen.c`, `codegen.h` | Intermediate code and MIPS assembly generation |
| `driver.c` | Compiler entry point and command-line option handling |
| `scanner-driver.c` | Separate scanner testing driver |
| `Makefile` | Compilation and cleanup targets |
