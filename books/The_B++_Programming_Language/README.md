# The B++ Programming Language

A learner's book for people who want to *write* B++. It is example-driven: you
learn by reading and running small, complete programs, then by doing the
exercises. It is written in the spirit and structure of Kernighan & Ritchie's
*The C Programming Language*, adapted to B++ — an **original work in that
tradition**, not a copy.

## Why a book shaped like K&R

B++ is, in spirit, a cousin of C. C grew out of **B** (Ken Thompson, 1969),
which was *typeless*: its only data type was the machine word. K&R is the book
Dennis Ritchie wrote once he had *added types to B*. B++ takes the other branch:
it keeps B's machine-word default (a bare `auto x` is just a word) and layers
*optional, smart* type hints on top. So this book teaches a language that sits
historically between B and C — and the places it leans toward B, rather than C,
are called out as we go.

## How to read it

Every program in the text is a real file under this book's `examples/`
directory. You need the `bpp` compiler from the repository this book ships in
(build or install it from the repo root). The commands below are run from the
book's own directory:

```sh
bpp examples/ch01/01_hello.bpp -o /tmp/hello
/tmp/hello
```

Programs that read input take it on standard input:

```sh
echo "hello world" | /tmp/hello        # pipe text in
/tmp/prog < some_file.txt              # or redirect a file
```

Each chapter ends with exercises. Reference solutions live under this book's
`tests/`, and most check themselves with `assert`, so a wrong answer fails
loudly. Compile and run the entire book — every example and every exercise —
with the conformance gate:

```sh
sh tests/run_book.sh
```

A listing that fails to compile, emits a warning, or exits non-zero fails the
gate. The book is only as trustworthy as the code that compiles from it.

## The "C → B++" boxes

Where B++ departs from C, a box names the difference so the lesson is explicit:

> **C → B++.** *The C way:* `printf("%d\n", n);` — *The B++ way:* `putnum(n);
> putchar('\n');`. *Why:* B++ has no format-string mini-language; you compose
> output by calling the right printer for each value.

These boxes are also the running list of the language's distinctive edges.

## Contents

1. [A Tutorial Introduction](ch01_tutorial.md) — your first programs:
   variables, loops, constants, character I/O, arrays, functions, parameters,
   character arrays, and scope.

*(Further chapters follow the K&R arc: Types & Expressions; Control Flow;
Functions & Program Structure; Pointers & Arrays; Structures; Input & Output;
the System Interface. See `docs/plans/bpp_book_plan.md` for the full map.)*
