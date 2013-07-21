//===-- Type.cpp - Fortran Type Interface ---------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The Fortran type interface.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/Type.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Expr.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

QualType
QualifierCollector::apply(const ASTContext &Context, QualType QT) const {
  return Context.getQualifiedType(QT, *this);
}

QualType
QualifierCollector::apply(const ASTContext &Context, const Type *T) const {
  return Context.getQualifiedType(T, *this);
}

//===----------------------------------------------------------------------===//
//                             Subtype Methods
//===----------------------------------------------------------------------===//

ArrayType::ArrayType(ASTContext &C, TypeClass tc,
                     QualType et, QualType can,
                     ArrayRef<ArraySpec*> dims)
  : Type(tc, can), ElementType(et) {
  DimCount = dims.size();
  Dims = new(C) ArraySpec*[DimCount];
  for(unsigned I = 0; I < DimCount; ++I)
    Dims[I] = dims[I];
}

ArrayType *ArrayType::Create(ASTContext &C, QualType ElemTy,
                             ArrayRef<ArraySpec*> Dims) {
  // forgot type alignment, what a day that was! full of debugging :/
  return new (C, TypeAlignment) ArrayType(C, Array, ElemTy, QualType(), Dims);
}

bool ArrayType::EvaluateSize(uint64_t &Result, const ASTContext &Ctx) const {
  Result = 1;
  auto Dimensions = getDimensions();
  for(size_t I = 0; I < Dimensions.size(); ++I) {
    int64_t LowerBound, UpperBound;
    if(!Dimensions[I]->EvaluateBounds(LowerBound, UpperBound, Ctx))
      return false;
    assert(LowerBound <= UpperBound);
    Result *= uint64_t(UpperBound - LowerBound + 1);
    // FIXME: overflow checks.
  }
  return true;
}

static const char * TypeKindStrings[] = {
  #define INTEGER_KIND(NAME, VALUE) #VALUE ,
  #define FLOATING_POINT_KIND(NAME, VALUE) #VALUE ,
  #include "flang/AST/BuiltinTypeKinds.def"
  "?"
};

const char *BuiltinType::getTypeKindString(TypeKind Kind) {
  return TypeKindStrings[Kind];
}

} //namespace flang
