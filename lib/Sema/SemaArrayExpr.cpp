//===- SemaArrayExpr.cpp - Array Expressions AST Builder and Sema  -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "fort/AST/ASTContext.h"
#include "fort/AST/Decl.h"
#include "fort/AST/Expr.h"
#include "fort/Basic/Diagnostic.h"
#include "fort/Sema/DeclSpec.h"
#include "fort/Sema/Sema.h"
#include "fort/Sema/SemaDiagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace fort {

/// DimensionConstructor - constructs a specification
/// for a dimension of a one dimensional array which is created
/// by joining multiple scalar expressions or arrays.
class DimensionConstructor {
  ASTContext &Context;
  uint64_t ResultingSize;
  bool IsConstSizeOnly;

public:
  DimensionConstructor(ASTContext &C)
      : Context(C), ResultingSize(0), IsConstSizeOnly(true) {}

  void JoinWith(const ArrayType *T);
  void JoinWith(const Expr *E);

  ArraySpec *CreateDimension();
};

void DimensionConstructor::JoinWith(const ArrayType *T) {
  uint64_t Size;
  if (T->EvaluateSize(Size, Context)) {
    ResultingSize += Size;
    return;
  }
  IsConstSizeOnly = false;
}

void DimensionConstructor::JoinWith(const Expr *E) {
  if (!IsConstSizeOnly)
    return;
  if (E->getType()->isArrayType())
    return JoinWith(E->getType()->asArrayType());
  ResultingSize++;
}

ArraySpec *DimensionConstructor::CreateDimension() {
  if (IsConstSizeOnly) {
    return ExplicitShapeSpec::Create(
        Context, IntegerConstantExpr::Create(Context, ResultingSize));
  }
  return DeferredShapeSpec::Create(Context);
}

// FIXME: Items can be implied do.
bool Sema::CheckArrayConstructorItems(ArrayRef<Expr *> Items,
                                      QualType &ResultingArrayType) {
  bool Result = true;
  size_t I;
  QualType ElementType;
  DimensionConstructor SpecConstructor(Context);

  // Set the first valid type to be the element type
  for (I = 0; I < Items.size(); ++I) {
    ElementType = Items[I]->getType();
    if (ElementType->isArrayType()) {
      CheckArrayExpr(Items[I]);
      ElementType = ElementType->asArrayType()->getElementType();
    }
    SpecConstructor.JoinWith(Items[I]);
    if (CheckTypeScalarOrCharacter(Items[I], ElementType, true)) {
      ++I;
      break;
    }
    Result = false;
  }

  // Constraint: Each ac-value expression in the array-constructor
  // shall have the same type and kind type parameter.
  // FIXME: Constraint: Each character value same length.
  for (; I < Items.size(); ++I) {
    auto T = Items[I]->getType();
    if (T->isArrayType()) {
      CheckArrayExpr(Items[I]);
      T = T->asArrayType()->getElementType();
    }
    SpecConstructor.JoinWith(Items[I]);
    if (!CheckTypesOfSameKind(ElementType, T, Items[I]))
      Result = false;
  }

  ResultingArrayType =
      Context.getArrayType(ElementType, SpecConstructor.CreateDimension());
  return Result;
}

ExprResult Sema::ActOnArrayConstructorExpr(ASTContext &C, SourceLocation Loc,
                                           SourceLocation RParenLoc,
                                           ArrayRef<Expr *> Elements) {
  QualType ReturnType;
  CheckArrayConstructorItems(Elements, ReturnType);
  return ArrayConstructorExpr::Create(C, Loc, Elements, ReturnType);
}

} // namespace fort
