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

  CharacterValueTy() {}
  CharacterValueTy(llvm::Value *Pointer, llvm::Value *Length)
    : Ptr(Pointer), Len(Length) {}
};

class ArrayDimensionValueTy {
public:
  llvm::Value *LowerBound;
  llvm::Value *UpperBound;
  llvm::Value *Stride;

  ArrayDimensionValueTy() {}
  ArrayDimensionValueTy(llvm::Value *LB, llvm::Value *UB = nullptr,
                        llvm::Value *Str = nullptr)
    : LowerBound(LB), UpperBound(UB), Stride(Str) {}

  bool hasLowerBound() const {
    return LowerBound != nullptr;
  }
  bool hasUpperBound() const {
    return UpperBound != nullptr;
  }
  bool hasStride() const {
    return Stride != nullptr;
  }
};

class LValueTy {
public:
  llvm::Value *Ptr, *Len;

  LValueTy() {}
  LValueTy(llvm::Value *Dest)
    : Ptr(Dest) {}
  LValueTy(llvm::Value *Dest, llvm::Value *Length)
    : Ptr(Dest), Len(Length) {}

  llvm::Value *getPointer() const {
    return Ptr;
  }
  llvm::Value *getLength() const {
    return Len;
  }
};

class RValueTy {
public:
  enum Kind {
    None,
    Scalar,
    Complex,
    Character
  };

  Kind ValueType;
  llvm::Value *V1;
  llvm::Value *V2;

  RValueTy() : ValueType(None) {}
  RValueTy(llvm::Value *V)
    : V1(V), ValueType(Scalar) {}
  RValueTy(ComplexValueTy C)
    : V1(C.Re), V2(C.Im), ValueType(Complex) {}
  RValueTy(CharacterValueTy CharValue)
    : V1(CharValue.Ptr), V2(CharValue.Len), ValueType(Character) {}

  Kind getType() const {
    return ValueType;
  }
  bool isScalar() const {
    return getType() == Scalar;
  }
  bool isComplex() const {
    return getType() == Complex;
  }
  bool isCharacter() const {
    return getType() == Character;
  }

  llvm::Value *asScalar() const {
    return V1;
  }
  ComplexValueTy asComplex() const {
    return ComplexValueTy(V1, V2);
  }
  CharacterValueTy asCharacter() const {
    return CharacterValueTy(V1, V2);
  }

};

}  // end namespace CodeGen
}  // end namespace flang

#endif
