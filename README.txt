//===----------------------------------------------------------------------===//
// Fortran Language Family Front-end
//===----------------------------------------------------------------------===//

fort:
  n. 1. A strong or fortified place.

Fort is a Fortran front-end.

Note: this is an "active developement" branch, expect breaking changes here or
upstream.

//===----------------------------------------------------------------------===//
// Compiling Fort (master branch)
//===----------------------------------------------------------------------===//

Download LLVM source.

> git clone https://github.com/llvm-mirror/llvm.git
> cd llvm

or

> git clone http://llvm.org/git/llvm.git
> cd llvm

Download the Fort source code within the LLVM tree, the two will be compiled
together.

> cd llvm/tools & git clone git://github.com/llvm-fortran/fort.git

Compile LLVM and Fort together.

> mkdir build
> cd build & cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
> cd build & make
> cd build & make install

You will also need to install the Fort standard library, which the Fort 
compiler links to by default. This is built outside the LLVM tree.

> git clone git://github.com/llvm-fortran/libfortrt.git
> cd libfortrt
> mkdir build
> cd build & cmake ../ -DCMAKE_INSTALL_PREFIX=/usr
> cd build & make
> cd build & make install

Testing:

> cmake -DLLVM_INCLUDE_TESTS=On ..
> make check-fort

If built within LLVM source tree, it should pick up test settings from it and
running a separate make command should not be necessary. If you want to build
tests in standalone fort build, make sure to add CMake flags that would
install FileCheck with LLVM:

> -DLLVM_INSTALL_UTILS=On -DLLVM_INCLUDE_TESTS=On

//===----------------------------------------------------------------------===//
// Using fort
//===----------------------------------------------------------------------===//

Fort's driver will instruct the linker to link with the libfort runtime. 
You can get libfort at https://github.com/llvm-fortran/libfortrt.
Once you have libfort, you'll need to tell fort where it is - you can use the
-L option (e.g. -L~/libfort).

//===----------------------------------------------------------------------===//
// Getting help
//===----------------------------------------------------------------------===//

Gitter chat: https://gitter.im/llvm-fortran/Lobby

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

* Preprocessor support
* Fort driver (?)
* Parsing GNU modules
* Add (or hoist) Clang style TargetInfo class template

Longer term:

* Fortran90/95 support
* IO support.
