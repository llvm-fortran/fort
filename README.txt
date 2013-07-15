//===----------------------------------------------------------------------===//
// Fortran Language Family Front-end
//===----------------------------------------------------------------------===//

flang:
  n. 1. A miner's two-pointed pick.

Flang is a Fortran front-end.

//===----------------------------------------------------------------------===//
// Compiling Flang (master branch)
//===----------------------------------------------------------------------===//

Flang depends on a fork of clang for some of its files. In order to compile
Flang, you'll need to merge the clang branch and flang branch together in your
local repository. Please use the following two commands to achieve this:

  git merge --squash --no-commit clang_branch
  git reset HEAD

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
