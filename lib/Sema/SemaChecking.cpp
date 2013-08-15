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
#include "flang/AST/ExprVisitor.h"
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

class ArgumentDependentExprChecker : public ExprVisitor<ArgumentDependentExprChecker> {
public:
  ASTContext &Context;
  Sema &Sem;
  DiagnosticsEngine &Diags;
  bool HasArgumentTypeErrors;
  bool CheckEvaluatable;
  bool ArgumentDependent;
  bool Evaluatable;

  ArgumentDependentExprChecker(ASTContext &C, Sema &S,
                               DiagnosticsEngine &Diag)
    : Context(C), Sem(S), Diags(Diag),
      HasArgumentTypeErrors(false),
      CheckEvaluatable(false), ArgumentDependent(false), Evaluatable(true) {}

  void VisitVarExpr(VarExpr *E) {
    auto VD = E->getVarDecl();
    if(VD->isArgument()) {
      ArgumentDependent = true;
      if(!CheckEvaluatable) {
        if(VD->getType().isNull()) {
          if(!Sem.ApplyImplicitRulesToArgument(const_cast<VarDecl*>(VD),
                                               E->getSourceRange())) {
            HasArgumentTypeErrors = true;
            return;
          }
          E->setType(VD->getType());
        }
        if(!VD->getType()->isIntegerType()) {
          Diags.Report(E->getLocation(), diag::err_array_explicit_shape_requires_int_arg)
            << VD->getType() << E->getSourceRange();
          HasArgumentTypeErrors = true;
        }
      }
    } else
      VisitExpr(E);
  }

  void VisitBinaryExpr(BinaryExpr *E) {
    Visit(E->getLHS());
    Visit(E->getRHS());
  }
  void VisitUnaryExpr(UnaryExpr *E) {
    Visit(E->getExpression());
  }
  void VisitImplicitCastExpr(ImplicitCastExpr *E) {
    Visit(E->getExpression());
  }
  void VisitExpr(Expr *E) {
    if(!E->isEvaluatable(Context)) {
      Evaluatable = false;
      if(CheckEvaluatable) {
        Diags.Report(E->getLocation(),
                     diag::err_expected_constant_expr)
          << E->getSourceRange();
      }
    }
  }
};

bool Sema::CheckArgumentDependentEvaluatableIntegerExpression(Expr *E) {
  ArgumentDependentExprChecker EV(Context, *this, Diags);
  EV.Visit(E);
  if(EV.ArgumentDependent) {
    /// Report unevaluatable errors.
    if(!EV.Evaluatable) {
      EV.CheckEvaluatable = true;
      EV.Visit(E);
    } else if(!EV.HasArgumentTypeErrors)
      CheckIntegerExpression(E);
    return true;
  }
  return false;
}

bool Sema::StatementRequiresConstantExpression(SourceLocation Loc, const Expr *E) {
  if(!E->isEvaluatable(Context)) {
    Diags.Report(Loc,
                 diag::err_stmt_requires_consant_expr)
      << E->getSourceRange();
    return false;
  }
  return true;
}

bool Sema::CheckIntegerExpression(const Expr *E) {
  if(E->getType()->isIntegerType())
    return true;
  Diags.Report(E->getLocation(),
               diag::err_typecheck_expected_integer_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::StmtRequiresIntegerExpression(SourceLocation Loc, const Expr *E) {
  if(E->getType()->isIntegerType())
    return true;
  Diags.Report(Loc, diag::err_typecheck_stmt_requires_int_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::CheckScalarNumericExpression(const Expr *E) {
  if(E->getType()->isIntegerType() ||
     E->getType()->isRealType())
    return true;
  Diags.Report(E->getLocation(),
               diag::err_expected_scalar_numeric_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::StmtRequiresIntegerVar(SourceLocation Loc, const VarExpr *E) {
  if(E->getType()->isIntegerType())
    return true;
  Diags.Report(Loc,
               diag::err_typecheck_stmt_requires_int_var)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::StmtRequiresScalarNumericVar(SourceLocation Loc, const VarExpr *E, unsigned DiagId) {
  if(E->getType()->isIntegerType() ||
     E->getType()->isRealType())
    return true;
  Diags.Report(Loc, DiagId)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::CheckLogicalExpression(const Expr *E) {
  if(E->getType()->isLogicalType())
    return true;
  Diags.Report(E->getLocation(),
               diag::err_typecheck_expected_logical_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::StmtRequiresLogicalExpression(SourceLocation Loc, const Expr *E) {
  if(E->getType()->isLogicalType())
    return true;
  Diags.Report(Loc, diag::err_typecheck_stmt_requires_logical_expr)
    << E->getType() << E->getSourceRange();
  return false;
}

bool Sema::StmtRequiresIntegerOrLogicalOrCharacterExpression(SourceLocation Loc, const Expr *E) {
  auto Type = E->getType();
  if(Type->isIntegerType() || Type->isLogicalType() || Type->isCharacterType())
    return true;
  Diags.Report(Loc, diag::err_typecheck_stmt_requires_int_logical_char_expr)
    << Type << E->getSourceRange();
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

Expr *Sema::TypecheckExprIntegerOrLogicalOrSameCharacter(Expr *E,
                                                         QualType ExpectedType) {
  auto GivenType = E->getType();
  if(ExpectedType->isIntegerType()) {
    if(CheckIntegerExpression(E)) {
      if(!CheckTypesSameKind(ExpectedType, GivenType))
        return ImplicitCastExpr::Create(Context, E->getLocation(), ExpectedType, E);
    }
  }
  else if(ExpectedType->isLogicalType()) {
    if(CheckLogicalExpression(E)) {
      if(!CheckTypesSameKind(ExpectedType, GivenType))
        return ImplicitCastExpr::Create(Context, E->getLocation(), ExpectedType, E);
    }
  } else {
    assert(ExpectedType->isCharacterType());
    if(!GivenType->isCharacterType() || !CheckTypesSameKind(ExpectedType, GivenType)) {
      Diags.Report(E->getLocation(),
                   diag::err_typecheck_expected_char_expr)
        << E->getType() << E->getSourceRange();
    }
  }
  return E;
}

bool Sema::IsDefaultBuiltinOrDoublePrecisionType(QualType T) {
  if(!T->isBuiltinType())
    return false;
  auto Ext = T.getExtQualsPtrOrNull();
  if(!Ext) return true;
  if(Ext->getKindSelector() == BuiltinType::NoKind ||
     Ext->isDoublePrecisionKind())
    return true;
  return false;
}

bool Sema::CheckDefaultBuiltinOrDoublePrecisionExpression(const Expr *E) {
  if(IsDefaultBuiltinOrDoublePrecisionType(E->getType()))
    return true;
  Diags.Report(E->getLocation(),
               diag::err_typecheck_expected_default_kind_expr)
    << E->getType() << E->getSourceRange();
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
  for(I = 0; I < Items.size(); ++I) {
    ElementType = Items[I]->getType();
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
                   diag::err_typecheck_expected_expr_of_type)
        << ElementType << T << Items[I]->getSourceRange();
      Result = false;
    }
  }

  ObtainedElementType = ElementType;
  return Result;
}

// FIXME: ARR(:) = ARR(:) or ARR(*)
bool Sema::CheckArrayDimensionsCompability(const ArrayType *LHS,
                                           const ArrayType *RHS,
                                           SourceLocation Loc,
                                           SourceRange LHSRange,
                                           SourceRange RHSRange) {
  if(LHS->getDimensionCount() !=
     RHS->getDimensionCount()) {
    Diags.Report(Loc, diag::err_typecheck_expected_array_of_dim_count)
      << unsigned(LHS->getDimensionCount())
      << unsigned(RHS->getDimensionCount())
      << LHSRange << RHSRange;
    return false;
  }

  for(size_t I = 0, Count = LHS->getDimensionCount(); I < Count; ++I) {
    auto LHSDim = LHS->getDimensions()[I];
    auto RHSDim = RHS->getDimensions()[I];
    int64_t LHSBounds[2], RHSBounds[2];
    if(LHSDim->EvaluateBounds(LHSBounds[0], LHSBounds[1], Context)) {
      if(RHSDim->EvaluateBounds(RHSBounds[0], RHSBounds[1], Context)) {
        auto LHSSize = LHSBounds[1] - LHSBounds[0] + 1;
        auto RHSSize = RHSBounds[1] - RHSBounds[0] + 1;
        if(LHSSize != RHSSize) {
          Diags.Report(Loc, diag::err_typecheck_array_dim_shape_mismatch)
           << int(LHSSize) << unsigned(I+1) << int(RHSSize)
           << LHSRange << RHSRange;
          return false;
        }
      }
    }
  }

  return true;
}

bool Sema::CheckVarIsAssignable(const VarExpr *E) {
  for(auto I : CurLoopVars) {
    if(I->getVarDecl() == E->getVarDecl()) {
      Diags.Report(E->getLocation(), diag::err_var_not_assignable)
        << E->getVarDecl()->getIdentifier() << E->getSourceRange();
      Diags.Report(I->getLocation(), diag::note_var_prev_do_use)
        << I->getSourceRange();
      return false;
    }
  }
  return true;
}

} // end namespace flang
