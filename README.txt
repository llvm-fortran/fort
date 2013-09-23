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
Flang, you'll need to fetch the clang_branch and merge it with master in your
local repository. After cloning flang, you can use the following commands to achieve this:
  
  git fetch origin clang_branch
  git checkout clang_branch
  git checkout master
  git merge --squash --no-commit clang_branch
  git reset HEAD

//===----------------------------------------------------------------------===//
// Using flang
//===----------------------------------------------------------------------===//

Flang's driver will instruct the linker to link with the libflang runtime. 
You can get libflang at https://github.com/hyp/libflang . Once you have libflang, 
you'll need to tell flang where it is - you can use the -L option (e.g. -L~/libflang).

//===----------------------------------------------------------------------===//
// To Do List
//===----------------------------------------------------------------------===//

Short term:

* Fix lexing bugs
  - Fixed form for numerical literals (i.e. ignore whitespace)
  - Continuations in BOZ literals
  - Others
* 'INCLUDE' which search for files in the directory of the current file first.
* Full parsing of statements

Long term:

* Flang driver (?)
* Parsing GNU modules

Longer term:

* Fortran90/95 support
* IO support.
