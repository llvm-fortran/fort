//===--- SemaChecking.cpp - Extra Semantic Checking -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "flang/Sema/Sema.h"
#include "flang/Sema/DeclSpec.h"
#include "flang/Sema/SemaDiagnostic.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/Basic/Diagnostic.h"
#include "llvm/Support/raw_ostream.h"

namespace flang {

int64_t Sema::EvalAndCheckIntExpr(const Expr *E,
                                  int64_t ErrorValue) {
  auto Type = E->getType();
  if(Type.isNull() || !Type->isIntegerType())
    goto error;
  int64_t Result;
  if(!E->EvaluateAsInt(Result, Context))
    goto error;

  return Result;
error:
  Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
    << E->getSourceRange();
  return ErrorValue;
}

int64_t Sema::CheckIntGT0(const Expr *E, int64_t EvalResult,
                          int64_t ErrorValue) {
  if(EvalResult < 1) {
    Diags.Report(E->getLocation(), diag::err_expected_integer_gt_0)
      << E->getSourceRange();
    return ErrorValue;
  }
  return EvalResult;
}

BuiltinType::TypeKind Sema::EvalAndCheckTypeKind(QualType T,
                                                 const Expr *E) {
  auto Result = EvalAndCheckIntExpr(E, 4);
  switch(cast<BuiltinType>(T.getTypePtr())->getTypeSpec()) {
  case BuiltinType::Integer:
  case BuiltinType::Logical: {
    switch(Result) {
    case 1: return BuiltinType::Int1;
    case 2: return BuiltinType::Int2;
    case 4: return BuiltinType::Int4;
    case 8: return BuiltinType::Int8;
    }
    break;
  }
  case BuiltinType::Real:
  case BuiltinType::Complex: {
    switch(Result) {
    case 4: return BuiltinType::Real4;
    case 8: return BuiltinType::Real8;
    }
    break;
  }
  default:
    llvm_unreachable("invalid type kind");
  }
  Diags.Report(E->getLocation(), diag::err_invalid_kind_selector)
    << int(Result) << T << E->getSourceRange();
  return BuiltinType::NoKind;
}

unsigned Sema::EvalAndCheckCharacterLength(const Expr *E) {
  auto Result = CheckIntGT0(E, EvalAndCheckIntExpr(E, 1));
  if(Result > int64_t(std::numeric_limits<unsigned>::max())) {
    //FIXME overflow diag
    Diags.Report(E->getLocation(), diag::err_expected_integer_constant_expr)
      << E->getSourceRange();
    return 1;
  }
  return Result;
}

bool Sema::CheckConstantExpression(const Expr *E) {
  if(!E->isEvaluatable(Context)) {
    Diags.Report(E->getLocation(),
                 diag::err_expected_constant_expr)
      << E->getSourceRange();
    return false;
  }
  return true;
}

bool Sema::CheckIntegerExpression(const Expr *E) {
  if(E->getType()->isIntegerType())
    return true;
  Diags.Report(E->getLocation(),
               diag::err_expected_integer_expr)
    << E->getSourceRange();
  return false;
}

bool Sema::CheckIntegerVar(const VarExpr *E) {
  if(E->getType()->isIntegerType())
    return true;
  Diags.Report(E->getLocation(),
               diag::err_typecheck_expected_int_var)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::CheckCharacterExpression(const Expr *E) {
  if(E->getType()->isCharacterType())
    return true;
  Diags.Report(E->getLocation(),
               diag::err_typecheck_expected_char_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::CheckTypesSameKind(QualType A, QualType B) const {
  if(auto ABTy = dyn_cast<BuiltinType>(A.getTypePtr())) {
    auto BBTy = dyn_cast<BuiltinType>(B.getTypePtr());
    if(!BBTy) return false;
    auto Spec = ABTy->getTypeSpec();
    if(Spec != BBTy->getTypeSpec()) return false;
    auto AExt = A.getExtQualsPtrOrNull();
    auto BExt = B.getExtQualsPtrOrNull();
    switch(Spec) {
    case BuiltinType::Integer:
      return Context.getIntTypeKind(AExt) ==
             Context.getIntTypeKind(BExt);
    case BuiltinType::Real:
      return Context.getRealTypeKind(AExt) ==
             Context.getRealTypeKind(BExt);
    case BuiltinType::Character:
      return true;
    case BuiltinType::Complex:
      return Context.getComplexTypeKind(AExt) ==
             Context.getComplexTypeKind(BExt);
    case BuiltinType::Logical:
      return Context.getLogicalTypeKind(AExt) ==
             Context.getLogicalTypeKind(BExt);
    }
  }
  return false;
}

bool Sema::CheckTypeScalarOrCharacter(const Expr *E, QualType T, bool IsConstant) {
  if(isa<BuiltinType>(T.getTypePtr())) return true;
  Diags.Report(E->getLocation(), IsConstant?
                 diag::err_expected_scalar_or_character_constant_expr :
                 diag::err_expected_scalar_or_character_expr)
    << E->getSourceRange();
  return false;
}

void Sema::CheckExpressionListSameTypeKind(ArrayRef<Expr*> Expressions) {
  assert(!Expressions.empty());
  auto T = Expressions.front()->getType();
  for(size_t I = 0; I < Expressions.size(); ++I) {
    auto E = Expressions[I];
    if(!CheckTypesSameKind(T, E->getType())) {
      Diags.Report(E->getLocation(), 0) // FIXME
        << E->getSourceRange();
    }
  }
}

static const BuiltinType *getBuiltinType(const Expr *E) {
  return dyn_cast<BuiltinType>(E->getType().getTypePtr());
}

/// Returns true if a type is a double precision real type (Kind is 8).
static bool IsTypeDoublePrecisionReal(QualType T) {
  auto Ext = T.getExtQualsPtrOrNull();
  return T->isRealType() &&
           Ext && Ext->getKindSelector() == BuiltinType::Real8? true : false;
}

/// Returns true if a type is a single precision real type (Kind is 4).
static bool IsTypeSinglePrecisionReal(QualType T) {
  if(T->isRealType())
    return !IsTypeDoublePrecisionReal(T);
  return false;
}

/// Returns true if a type is a double precision complex type (Kind is 8).
static bool IsTypeDoublePrecisionComplex(QualType T) {
  auto Ext = T.getExtQualsPtrOrNull();
  return T->isComplexType() &&
           Ext && Ext->getKindSelector() == BuiltinType::Real8? true : false;
}

bool Sema::CheckIntegerArgument(const Expr *E) {
  auto Type = getBuiltinType(E);
  if(!Type || !Type->isIntegerType()) {
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << E->getType() << Context.IntegerTy
      << E->getSourceRange();
    return true;
  }
  return false;
}

bool Sema::CheckRealArgument(const Expr *E) {
  auto Type = getBuiltinType(E);
  if(!Type || !Type->isRealType()) {
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << E->getType() << Context.RealTy
      << E->getSourceRange();
    return true;
  }
  return false;
}

bool Sema::CheckComplexArgument(const Expr *E) {
  auto Type = getBuiltinType(E);
  if(!Type || !Type->isComplexType()) {
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << E->getType() << Context.ComplexTy
      << E->getSourceRange();
    return true;
  }
  return false;
}

bool Sema::CheckStrictlyRealArgument(const Expr *E) {
  if(!IsTypeSinglePrecisionReal(E->getType())) {
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << E->getType() << Context.RealTy
      << E->getSourceRange();
    return true;
  }
  return false;
}

bool Sema::CheckDoublePrecisionRealArgument(const Expr *E) {
  if(!IsTypeDoublePrecisionReal(E->getType())) {
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << E->getType() << Context.DoublePrecisionTy
      << E->getSourceRange();
    return true;
  }
  return false;
}

bool Sema::CheckDoubleComplexArgument(const Expr *E) {
  if(!IsTypeDoublePrecisionComplex(E->getType())) {
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << E->getType() << Context.DoubleComplexTy
      << E->getSourceRange();
    return true;
  }
  return false;
}

bool Sema::CheckCharacterArgument(const Expr *E) {
  auto Type = getBuiltinType(E);
  if(!Type || !Type->isCharacterType()) {
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << E->getType() << Context.CharacterTy
      << E->getSourceRange();
  }
  return false;
}

bool Sema::CheckIntegerOrRealArgument(const Expr *E) {
  auto Type = getBuiltinType(E);
  if(!Type || !Type->isIntegerOrRealType()) {
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << E->getType() << "'integer' or 'real'"
      << E->getSourceRange();
    return true;
  }
  return false;
}

bool Sema::CheckIntegerOrRealOrComplexArgument(const Expr *E) {
  auto Type = getBuiltinType(E);
  if(!Type || !Type->isIntegerOrRealOrComplexType()) {
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << E->getType() << "'integer' or 'real' or 'complex'"
      << E->getSourceRange();
    return true;
  }
  return false;
}

bool Sema::CheckRealOrComplexArgument(const Expr *E) {
  auto Type = getBuiltinType(E);
  if(!Type || !Type->isRealOrComplexType()) {
    Diags.Report(E->getLocation(), diag::err_typecheck_passing_incompatible)
      << E->getType() << "'real' or 'complex'"
      << E->getSourceRange();
    return true;
  }
  return false;
}

// FIXME: Items can be implied do and other array constructors..
bool Sema::CheckArrayConstructorItems(ArrayRef<Expr*> Items,
                                      QualType &ObtainedElementType) {
  if(Items.empty()) return true;

  bool Result = true;
  size_t I;
  QualType ElementType;
  // Set the first valid type to be the element type
  for(size_t I = 0; I < Items.size(); ++I) {
    auto ElementType = Items[I]->getType();
    if(CheckTypeScalarOrCharacter(Items[I], ElementType, true))
      break;
    Result = false;
  }

  // NB: ignore character value same length constraint.
  // Constraint: Each ac-value expression in the array-constructor
  // shall have the same type and kind type parameter.
  for(; I < Items.size(); ++I) {
    auto T = Items[I]->getType();
    if(!CheckTypesSameKind(ElementType, T)) {
      Diags.Report(Items[I]->getLocation(),
                   diag::err_typecheck_array_constructor_invalid_item)
        << Items[I]->getSourceRange();
      Result = false;
    }
  }

  ObtainedElementType = ElementType;
  return Result;
}

} // end namespace flang
