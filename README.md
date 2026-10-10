# Smalltalk in CREXX

A Smalltalk-80 interpreter written in [CREXX](https://github.com/adesutherland/CREXX) (Level B).
It implements the Smalltalk-80 core and a class library, and reads programs in the
GNU Smalltalk bracket syntax.

```
./smalltalk examples/hello.st
echo "(3/4 + 0.25) printNl." | ./smalltalk
```

The launcher compiles the interpreter to bytecode on first use (about half a minute) and runs
it afterwards. Set `CREXX` to the `crexx` driver if it is not on your `PATH`.

## What is implemented

* **Language**: classes and metaclasses, instance/class variables, class-side instance variables,
  inheritance, `super`, blocks (closures) with non-local `^` return, cascades, `{ }` brace arrays,
  literal arrays. Control flow (`ifTrue:`, `whileTrue:`, `to:do:`,
  `and:`, `timesRepeat:` ...) is inlined when the arguments are literal blocks.
* **Syntax**: `Object subclass: Foo [ |a b| method [ ... ] Foo class >> sel [ ... ] ]`,
  `Foo extend [ ]`, `Foo class extend [ ]`, `Foo >> sel [ ]`, `Eval [ ]`, plain top-level
  statements, and the message form `Object subclass: #Foo instanceVariableNames: ... package: ...`.
* **Numbers**: SmallInteger, arbitrary-size integers, Fractions (exact), Floats, mixed arithmetic
  with coercion, radix literals (`16r1F`), `\\ // rem: quo: gcd: factorial raisedTo:` and more.
* **Collections**: Array, OrderedCollection, SortedCollection, Interval, Set, Bag, Dictionary,
  Association, String, Symbol, with the usual `do:`/`collect:`/`select:`/`inject:into:` protocol.
* **Data structures**: Complex numbers, Stack, Queue, Matrix; number formatting (`printShowingDecimalPlaces:`, `asStringWithCommas`).
* **Streams**: ReadStream, WriteStream, ReadWriteStream, FileStream (`read:`, `write:`, `append:`, `exists:`); **Date** and **Time** (`today`, `now`, **DateTime**, calendar arithmetic); `Transcript` (`show:`, `showCr:`, `display:`,
  `print:`, `cr`, `tab`...).
* **Exceptions**: `on:do:`, `ensure:`, `ifCurtailed:`, `signal`, `signal:`, `retry`, `return:`,
  `resume:`, `pass`, `outer`, `retryUsing:`, exception sets (`ZeroDivide, Error`), user-defined
  `Error` subclasses, and gst-style traces for unhandled errors.
* **Reflection**: `class`, `superclass`, `respondsTo:`, `isKindOf:`, `perform:`, `selectors`,
  `instanceVariableNames`, `Smalltalk at:put:`.

## Differences from other Smalltalks

* `new` does **not** call `initialize` (as in Smalltalk-80 and GNU Smalltalk 3.2).
* Dictionary/Set keys use built-in equality (numbers, strings, symbols, characters, arrays of
  those) or identity for other objects; a user-defined `=`/`hash` is not consulted.
* `Dictionary printNl` shows `a Dictionary (k->v )`; Sets and Dictionaries iterate in insertion order.
* There is no garbage collector; objects live until the program ends.
* No processes, no `thisContext`, no class-definition bang (`!`) chunk format.

## Manual

`docs/Smalltalk-CREXX-Guide.pdf` (also as `.docx`) is the language reference and user's guide, 35 pages.

## Layout

| Path | Contents |
| --- | --- |
| `src/*.crexx` | interpreter sources (lexer/parser, resolver, evaluator, primitives, big integers) |
| `build.py` | concatenates `src/` into `smalltalk.crexx` and substitutes constants |
| `lib/*.st` | class library written in Smalltalk (kernel, numbers, collections, streams, exceptions, extras, structures) |
| `examples/` | sample programs |
| `tests/` | regression tests (`tests/run_tests.sh`) |
| `docs/` | user manual (docx and PDF) |

Rebuild after editing `src/`: `python3 build.py` (the launcher recompiles automatically).
Run the tests with `tests/run_tests.sh`.
