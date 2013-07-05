//===-- CGValue.h - LLVM CodeGen wrappers for llvm::Value* ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// These classes implement wrappers around llvm::Value in order to
// fully represent the range of values for Complex, Character, Array and L/Rvalues.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_CGVALUE_H
#define FLANG_CODEGEN_CGVALUE_H

#include "flang/AST/ASTContext.h"
#include "flang/AST/Type.h"
#include "llvm/IR/Value.h"

namespace llvm {
  class Constant;
  class MDNode;
}

namespace flang {
namespace CodeGen {

class ComplexValueTy {
public:
  llvm::Value *Re, *Im;

  ComplexValueTy() {}
  ComplexValueTy(llvm::Value *Real, llvm::Value *Imaginary)
    : Re(Real), Im(Imaginary) {}

  inline llvm::Value *getPolarLength() const {
    return Re;
  }
  inline llvm::Value *getPolarTheta() const {
    return Im;
  }
};

class CharacterValueTy {
public:
  llvm::Value *Ptr, *Len;
};

class LValueTy {
public:
  llvm::Value *Ptr;

  LValueTy() {}
  LValueTy(llvm::Value *Dest)
    : Ptr(Dest) {}
  llvm::Value *getPointer() const {
    return Ptr;
  }
};

class RValueTy {
public:
  llvm::Value *V1;
  llvm::Value *V2;
};

}  // end namespace CodeGen
}  // end namespace flang

#endif
