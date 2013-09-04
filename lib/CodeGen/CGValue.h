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
                        llvm::Value *stride = nullptr)
    : LowerBound(LB), UpperBound(UB), Stride(stride) {}

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

/// \brief Rerpresents a single-element section of an array.
class ArrayElementSection {
public:
  llvm::Value *Index;

  ArrayElementSection() {}
  ArrayElementSection(llvm::Value *index)
    : Index(index) {}
};

/// \brief Represents a vector section of an array.
class ArrayVectorSection {
public:
  llvm::Value *Ptr;
  llvm::Value *Size;

  ArrayVectorSection() {}
  ArrayVectorSection(llvm::Value *P, llvm::Value *Len)
    : Ptr(P), Size(Len) {}
};

class ArraySection {
  enum Kind {
    Range, Element, Vector
  };
  Kind Type;
  llvm::Value *V1, *V2;
public:

  ArraySection()
    : Type(Range) {}
  ArraySection(const ArrayElementSection &S)
    : Type(Element), V1(S.Index) {}
  ArraySection(const ArrayVectorSection &S)
    : Type(Vector), V1(S.Ptr), V2(S.Size) {}

  bool isRangeSection() const {
    return Type == Range;
  }
  bool isElementSection() const {
    return Type == Element;
  }
  bool isVectorSection() const {
    return Type == Vector;
  }

  ArrayElementSection getElementSection() const {
    assert(isElementSection());
    return ArrayElementSection(V1);
  }
  ArrayVectorSection getVectorSection() const {
    assert(isVectorSection());
    return ArrayVectorSection(V1, V2);
  }
};

class ArrayValueTy {
public:
  ArrayRef<ArrayDimensionValueTy> Dimensions;
  ArrayRef<ArraySection> Sections;
  llvm::Value *Ptr;
  llvm::Value *Offset;

  ArrayValueTy(ArrayRef<ArrayDimensionValueTy> Dims,
               ArrayRef<ArraySection> sections,
               llvm::Value *P,
               llvm::Value *offset = nullptr)
    : Dimensions(Dims), Sections(sections), Ptr(P),
      Offset(offset) {}

  bool hasOffset() const {
    return Offset != nullptr;
  }
};

class LValueTy {
public:
  llvm::Value *Ptr;
  QualType Type;

  LValueTy() {}
  LValueTy(llvm::Value *Dest)
    : Ptr(Dest) {}
  LValueTy(llvm::Value *Dest, QualType Ty)
    : Ptr(Dest), Type(Ty) {}

  llvm::Value *getPointer() const {
    return Ptr;
  }
  QualType getType() const {
    return Type;
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
  bool isNothing() const {
    return getType() == None;
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
