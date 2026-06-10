# Chapter 1 — A Tutorial Introduction

The fastest way into a language is to write small programs in it. This chapter
walks through the core of B++ — output, variables, loops, decisions, functions,
arrays, and scope — one short, complete program at a time. Nothing here is the
whole story; each idea returns in a later chapter with the corners filled in.
The goal now is to get you writing and running real B++ quickly.

Every program below is a file under this book's `examples/ch01/`. Build and run
each one as you read (the commands are run from the book's directory, with the
`bpp` compiler from the repository this book ships in):

```sh
bpp examples/ch01/01_hello.bpp -o /tmp/p
/tmp/p
```

---

## 1.1 Getting started

The first program prints a line of text.

```
main() {
    put("Hello, B++\n");
}
```

A B++ program is a collection of *functions*. Execution begins in the one named
`main`. A function's body is a list of statements between braces; this one has a
single statement that calls `put` with a string. The `\n` is the newline
character. When `main` runs off the end of its body it returns 0 — the
success code — automatically, so we write no `return`.

`put` is the friendly output helper. Handed a string, it writes the text as-is.
Build the file, run it, and you get one line: `Hello, B++`.

> **C → B++.** There is no `#include` and no `import` at the top of the file.
> `put`, `putnum`, `getchar`, and the rest of the everyday toolbox are part of
> the *prelude* — about two dozen small libraries the compiler makes visible in
> every program. You call them as if they were built in.

---

## 1.2 Variables and arithmetic

Next, a table: the integers 1 through 8, each with its square and its cube
(`examples/ch01/02_squares_while.bpp`).

```
main() {
    auto n, last;
    n = 1;
    last = 8;

    while (n <= last) {
        putnum(n);          putchar('\t');
        putnum(n * n);      putchar('\t');
        putnum(n * n * n);  putchar('\n');
        n = n + 1;
    }
}
```

`auto n, last;` declares two variables. In B++ a plain `auto` variable is a
64-bit machine word — a signed integer for our purposes here. (Chapter 2 shows
how a type *hint* turns a word into a float, a byte, and so on.)

The `while` loop runs its body as long as the condition `n <= last` is true.
Each pass prints a row and then advances `n`. `putchar` writes a single
character — here a tab (`'\t'`) between columns and a newline at the end.

Notice we print numbers with `putnum`, not `put`. That is deliberate:

> **C → B++.** `put` is *smart*: it picks a printer from the **type** of its
> argument, at compile time, and when the type is known it just works and stays
> quiet — a string literal (or a `: ptr` value) prints as text, a `: float`
> prints with a fraction. The one ambiguous case is a plain `auto` variable: it
> is only a machine word, which could be an integer *or* a pointer to text, so
> `put(n)` on such a value prints it as a number **and** raises a warning (W032)
> asking you to confirm. The dispatch is not failing there — it is the
> optional-typing trade-off in action (Chapter 2 returns to it). To stay both
> clear and quiet, we name the printer directly throughout this book: `putnum`
> for a number, `putstr` for a string, `putchar` for one character.

---

## 1.3 The `for` statement

The loop in §1.2 has three moving parts: it starts `n` at 1, continues while
`n <= 8`, and steps `n` by 1. A `for` loop gathers those three onto one line
(`03_squares_for.bpp`):

```
main() {
    auto n;

    for (n = 1; n <= 8; n = n + 1) {
        putnum(n);          putchar('\t');
        putnum(n * n);      putchar('\t');
        putnum(n * n * n);  putchar('\n');
    }
}
```

The three clauses are *initialise*, *test*, and *advance*. The output is
identical to the `while` version. Use whichever reads better: a `for` when the
loop is plainly counted, a `while` when the continuation is a more general
condition.

---

## 1.4 Symbolic constants

The numbers `1` and `8` in the loop are *magic numbers* — bare values whose
meaning lives only in your head. Give them names with `const` at file scope
(`04_squares_const.bpp`):

```
const LOWER = 1;    // first value in the table
const UPPER = 8;    // last value in the table
const STEP  = 1;    // distance between rows

main() {
    auto n;
    for (n = LOWER; n <= UPPER; n = n + STEP) {
        putnum(n);          putchar('\t');
        putnum(n * n);      putchar('\t');
        putnum(n * n * n);  putchar('\n');
    }
}
```

A `const` gives a value a name. The name is replaced by the value at compile
time, so it costs nothing while the program runs, and the table's range now
changes by editing one labelled line instead of hunting for stray numbers.

---

## 1.5 Character input and output

Most useful programs read input. The lowest-level reader is `getchar`, which
returns the next byte of standard input as a value 0–255, or **−1** when the
input is exhausted. The matching writer is `putchar`. With just those two we
can copy input to output (`05_echo.bpp`):

```
main() {
    auto c;

    c = getchar();
    while (c != -1) {
        putchar(c);
        c = getchar();
    }
}
```

Read a byte; if it is not the end-of-input marker, write it and read again. Run
it and type; what you type is echoed back. Feed it a file instead:

```sh
bpp examples/ch01/05_echo.bpp -o /tmp/echo
/tmp/echo < examples/ch01/05_echo.bpp     # the program echoes its own source
```

> **C → B++.** C marks end of input with the named constant `EOF`. B++ uses the
> plain value **−1** returned by `getchar`. Write the loop against `-1`
> directly, or give it your own name with a `const` if you prefer.

This read loop is a skeleton you fill differently for each job. **Counting
characters** (`06_count_chars.bpp`) keeps a tally instead of echoing:

```
    c = getchar();
    while (c != -1) {
        count = count + 1;
        c = getchar();
    }
    putnum(count);
    putchar('\n');
```

**Counting lines** (`07_count_lines.bpp`) counts the newline characters, since
each line ends with one:

```
        if (c == '\n') {
            lines = lines + 1;
        }
```

**Counting words** (`08_count_words.bpp`) is the first program that has to
*remember* something between characters. A word is a run of non-blank
characters; we count one each time we step from a blank region into a word. The
memory is a small state variable:

```
const IN  = 1;   // the scanner is inside a word
const OUT = 0;   // the scanner is between words

    while (c != -1) {
        if (c == ' ' || c == '\n' || c == '\t') {
            state = OUT;
        } else if (state == OUT) {
            state = IN;
            words = words + 1;
        }
        c = getchar();
    }
```

`||` is logical OR, `&&` is logical AND, and `else if` chains decisions. This
*state machine* pattern — track which mode you are in, act on transitions —
recurs constantly in input processing.

---

## 1.6 Arrays

To count *each* digit separately we need ten counters. An array holds them in
one indexed block (`09_char_classes.bpp`):

```
    digits = buf_word(10);
    for (i = 0; i < 10; i = i + 1) {
        digits[i] = 0;
    }
```

`buf_word(10)` reserves ten word-sized slots; `digits[k]` reads or writes slot
`k`. We index by the digit's value, which we get by subtracting the character
`'0'` from the character code:

```
        if (c >= '0' && c <= '9') {
            digits[c - '0'] = digits[c - '0'] + 1;
        } else if (c == ' ' || c == '\n' || c == '\t') {
            white = white + 1;
        } else {
            other = other + 1;
        }
```

Because the characters `'0'` through `'9'` have consecutive codes, `c - '0'`
maps the character `'7'` to the index `7`. Arrays in B++ are blocks of memory
you ask for explicitly; Chapter 5 returns to them in full.

---

## 1.7 Functions

A function names a piece of work so you can reuse it and keep `main` readable.
Here is one that raises a number to a power (`10_power.bpp`):

```
power(base, n) {
    auto result, i;
    result = 1;
    for (i = 0; i < n; i = i + 1) {
        result = result * base;
    }
    return result;
}

main() {
    auto i;
    for (i = 0; i < 10; i = i + 1) {
        putnum(i);           putchar('\t');
        putnum(power(2, i)); putchar('\t');
        putnum(power(3, i)); putchar('\n');
    }
}
```

`power` takes two parameters, does its work in a private local `result`, and
hands one value back with `return`. `main` then calls it as often as it likes.
A function must be defined before — above — the code that calls it.

---

## 1.8 Arguments — call by value

B++ passes arguments *by value*: when you call a function, each parameter is a
private copy of the value the caller passed in. The function may use that
parameter as an ordinary working variable — read it, and change it — without
ever disturbing the caller. Only the `return` value carries a result back out
(`11_parameters.bpp`):

```
countdown_sum(n) {
    auto sum;
    sum = 0;
    while (n > 0) {
        sum = sum + n;
        n = n - 1;        // changing the parameter is fine — it is our copy
    }
    return sum;
}

main() {
    auto k, total;
    k = 5;
    total = countdown_sum(k);

    putstr("total = "); putnum(total); putchar('\n');   // 5+4+3+2+1 = 15
    putstr("k     = "); putnum(k);     putchar('\n');   // still 5
}
```

`countdown_sum` counts its own parameter `n` down to zero while it adds up the
total. That `n` is the function's private copy, so the caller's `k` is exactly
5 before and after the call. Because a parameter is your own copy, decrementing
it like a loop counter is a common and perfectly safe idiom.

> **C → B++.** Call by value works the same way it does in C: assigning to a
> parameter changes only the function's copy, never the caller's variable. To
> let a function modify a caller's variable you pass a *pointer* to it — pointers
> are Chapter 5.

That said, a function that quietly mutates its parameters can still be harder to
read. When a parameter stands for an input you want to keep intact, leaving it
untouched and working in a separate local is often the clearer choice — a matter
of style now, not correctness.

---

## 1.9 Character arrays

A line of text is stored in a *byte buffer* — an array of bytes. The program
below reads input line by line and prints the longest line it has seen
(`12_longest_line.bpp`). `get_line` fills a buffer one byte at a time with
`write_u8(buf, i, value)` and finishes with a 0 so the text can be printed by
`putstr`:

```
const MAXLINE = 1000;

get_line(buf, limit) {
    auto c, i;
    i = 0;
    c = getchar();
    while (c != -1 && c != '\n') {
        if (i < limit - 1) { write_u8(buf, i, c); }
        i = i + 1;
        c = getchar();
    }
    if (c == '\n') {
        if (i < limit - 1) { write_u8(buf, i, '\n'); }
        i = i + 1;
    }
    write_u8(buf, i, 0);   // terminate so putstr knows where to stop
    return i;
}
```

`main` keeps two buffers — the current line and the longest so far — and copies
the current line aside with `buf_move` whenever it sets a new record:

```
    len = get_line(line, MAXLINE);
    while (len > 0) {
        if (len > max) {
            max = len;
            buf_move(longest, line, len + 1);   // copy text and the 0
        }
        len = get_line(line, MAXLINE);
    }
```

`buf_byte(n)` reserves `n` bytes; `write_u8` / `read_u8` store and load one byte
at an offset; `buf_move(dst, src, n)` copies a block. A B++ string is just such
a buffer with a 0 byte marking the end.

---

## 1.10 External variables and scope

The variables we have used so far are *local*: they exist only inside the
function that declares them and vanish when it returns. A variable declared with
`global` at file scope is different — it exists for the whole run and every
function in the file can see it. Use one for state that several functions share,
instead of passing it through call after call (`13_scope_globals.bpp`):

```
global g_max;       // length of the longest line seen so far
global g_longest;   // buffer holding that line

save_longest(line, len) {
    if (len > g_max) {
        g_max = len;
        buf_move(g_longest, line, len + 1);
    }
}
```

Here `save_longest` reads and updates `g_max` and `g_longest` directly, with no
parameters for them at all. Shared globals are convenient but easy to overuse:
when only one or two functions touch a value, a parameter and a return are
usually clearer. `global` (and its file-private cousin `static`) is a *storage
class* — Chapter 4 is devoted to them.

---

## Chapter notes — where B++ leans away from C

The "C → B++" boxes collected:

- **No `#include` / `import` for everyday tools** — they come from the prelude
  (§1.1).
- **Explicit printers** — `putnum` / `putstr` / `putchar` rather than `printf`
  with format strings; smart `put` warns on a bare number (§1.2).
- **End of input is −1**, not a named `EOF` (§1.5).
- **Arguments are call by value** — just like C: a parameter is a private copy,
  and to modify a caller's variable you pass a pointer (Chapter 5) (§1.8).

---

## Exercises

Reference solutions are under `tests/ch01/`; most check themselves with
`assert`. Run the whole set with `sh tests/run_book.sh`.

1. **Temperature table.** Print a Celsius-to-Fahrenheit table from 0 to 100 in
   steps of 10, using a conversion function. (`ex01_temperature.bpp`)
2. **Sum to n.** Write a function that adds the integers 1..n and check it
   against the total for 100. (`ex02_sum_to.bpp`)
3. **GCD.** Compute the greatest common divisor of two numbers with Euclid's
   algorithm. (`ex03_gcd.bpp`)
4. **Primes.** Write `is_prime` and count the primes below 30.
   (`ex04_primes.bpp`)
5. **Reverse digits.** Reverse the decimal digits of a non-negative number using
   `% 10` and `/ 10`. (`ex05_reverse_digits.bpp`)
6. **Fibonacci.** Compute the n-th Fibonacci number iteratively.
   (`ex06_fib.bpp`)
7. **Squeeze blanks.** Copy input to output, replacing each run of blanks with a
   single space. (`ex07_squeeze_blanks.bpp`)
