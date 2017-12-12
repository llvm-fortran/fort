//===--- LLVM.h - Import various common LLVM datatypes ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file forward declares and imports various common LLVM datatypes that
// flang wants to use unqualified.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_BASIC_LLVM_H__
#define FLANG_BASIC_LLVM_H__

// This should be the only #include, force #includes of all the others on
// clients.
#include "llvm/Support/Casting.h"

namespace llvm {
  // ADT's.
  class APInt;
  class APFloat;
  class StringRef;
  class Twine;
#if 0
  template<typename T> class ArrayRef;
  template<typename T, unsigned N> class SmallVector;
  template<typename T> class SmallVectorImpl;
#endif
  template<typename T> class ArrayRef;
  template<unsigned InternalLen> class SmallString;
  template<typename T, unsigned N> class SmallVector;
  template<typename T> class SmallVectorImpl;
  template<typename T> class Optional;

  class raw_ostream;
  class raw_pwrite_stream;
  // TODO: DenseMap, ...
}


namespace flang {
  // Casting operators.
  using llvm::isa;
  using llvm::cast;
  using llvm::dyn_cast;
  using llvm::dyn_cast_or_null;
  using llvm::cast_or_null;
  
  // ADT's.
  using llvm::APInt;
  using llvm::APFloat;
  using llvm::StringRef;
  using llvm::Twine;
  using llvm::ArrayRef;
  using llvm::SmallString;
  using llvm::SmallVector;
  using llvm::SmallVectorImpl;
  
  using llvm::raw_ostream;
  using llvm::raw_pwrite_stream;
} // end namespace flang.

#endif
