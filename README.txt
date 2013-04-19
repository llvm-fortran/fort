//===----------------------------------------------------------------------===//
// Fortran Language Family Front-end
//===----------------------------------------------------------------------===//

flang:
  n. 1. A miner's two-pointed pick.

Flang is a Fortran front-end.

//===----------------------------------------------------------------------===//
// To Do List
//===----------------------------------------------------------------------===//

Short term:

* Fix lexing bugs
  - Continuations in BOZ literals
  - Others
* 'INCLUDE' which search for files in the directory of the current file first.
* Full parsing of statements
* Clang-style diagnostics
  - Warnings and notes
  - Ranges for errors (i.e. ^~~~~~ )
* Testing infrastructure

Long term:

* Flang driver (?)
* Parsing GNU modules
* Sema
* Code generation
* Builtin functions

Longer term:

* Fortran77 support
