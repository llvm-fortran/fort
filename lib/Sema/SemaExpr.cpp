//===- SemaExpr.cpp - Expression AST Builder and Semantic Analysis Implementation -===//
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
#include <sstream>

namespace flang {

enum {
  // emulate real(kind=8) as double precision for fortran 77 compability.
  TST_doubleprecision = TST_struct + 1
};

/// Returns TST_integer/TST_real/TST_complex if a given type
/// is an arithmetic type, or TST_unspecified otherwise
static TypeSpecifierType GetArithmeticTypeSpec(QualType T) {
  if(T->isIntegerType()) return TST_integer;
  else if(T->isRealType()) return TST_real;
  else if(T->isComplexType()) return TST_complex;
  else return TST_unspecified;
}

/// Returns true if a real type is also a double precision type.
static bool IsRealTypeDoublePrecision(QualType T) {
  auto Ext = T.getExtQualsPtrOnNull();
  return Ext && Ext->getArithmeticKindSelector() == 8? true : false;
}

/// Returns TST_integer/TST_real/TST_complex/TST_doubleprecision
/// if a given type is an arithmetic type, or TST_unspecified
/// otherwise
static int GetArithmeticTypeSpecFortran77(QualType T) {
  if(T->isIntegerType()) return TST_integer;
  else if(T->isRealType()) {
    auto Ext = T.getExtQualsPtrOnNull();
    return Ext && Ext->getArithmeticKindSelector() == 8?
             TST_doubleprecision : TST_real;
  }
  else if(T->isComplexType()) return TST_complex;
  else return TST_unspecified;
}


static llvm::APFloat GetNumberConstant(DiagnosticsEngine &Diags, Expr *E) {
  if(auto Real = dyn_cast<RealConstantExpr>(E)) {
    return Real->getValue();
  } else if(auto Int = dyn_cast<IntegerConstantExpr>(E)) {
    // FIXME: take count into account?
    llvm::APFloat result(llvm::APFloat::IEEEsingle);
    result.convertFromAPInt(Int->getValue(),true,llvm::APFloat::rmNearestTiesToEven);
    return result;
  } else if(auto Unary = dyn_cast<UnaryExpr>(E)) {
    auto Value = GetNumberConstant(Diags, Unary->getExpression());
    if(Unary->getOperator() == UnaryExpr::Minus)
      Value.changeSign();
    return Value;
  } else {
    Diags.Report(E->getLocation(), diag::err_expected_integer_or_real_constant_expr)
     << E->getSourceRange();
    return llvm::APFloat(0.0);
  }
}

/// Returns true if two arithmetic type qualifiers have the same kind
static bool ExtQualsSameKind(const ExtQuals *A, const ExtQuals *B) {
  auto AK = A? A->getArithmeticKindSelector() : ExtQuals::DefaultArithmeticKind;
  auto BK = B? B->getArithmeticKindSelector() : ExtQuals::DefaultArithmeticKind;
  return AK == BK;
}

/// Returns the largest kind between two arithmetic type qualifiers.
static int GetLargestKind(const ExtQuals *A, const ExtQuals *B) {
  auto AK = A? A->getArithmeticKindSelector() : ExtQuals::DefaultArithmeticKind;
  auto BK = B? B->getArithmeticKindSelector() : ExtQuals::DefaultArithmeticKind;
  return AK >= BK ? 0 : 1;
}

/// Creates an implicit cast expression
static Expr *ImplicitCast(ASTContext &C, QualType T, ExprResult E) {
  return ImplicitCastExpr::Create(C, E.get()->getLocation(), T, E.take());
}

/// Selects the type with the biggest kind from two arithmetic types,
/// applies any required conversions to that type for two expressions,
/// and returns that type.
static QualType SelectLargestKindApplyConversions(ASTContext &C,
                                                  ExprResult &A, ExprResult &B,
                                                  QualType AType, QualType BType) {
  const ExtQuals *AExt = AType.getExtQualsPtrOnNull();
  const ExtQuals *BExt = BType.getExtQualsPtrOnNull();
  auto AK = AExt? AExt->getArithmeticKindSelector() : ExtQuals::DefaultArithmeticKind;
  auto BK = BExt? BExt->getArithmeticKindSelector() : ExtQuals::DefaultArithmeticKind;
  if(AK == BK) return AType;
  else if(AK >= BK) {
    B = ImplicitCast(C, AType, B);
    return AType;
  } else {
    A = ImplicitCast(C, BType, A);
    return BType;
  }
}

/// Chooses a type from two arithmetic types,
/// and if another type has larger kind, expands the
/// chosen type to the larger kind.
/// Applies any required conversions to the chosen type for two expressions,
/// and returns the chosen type.
static QualType TakeTypeSelectLargestKindApplyConversion(ASTContext &C,
                                                         int Chosen,
                                                         ExprResult &A, ExprResult &B,
                                                         QualType AType, QualType BType) {
  QualType ChosenType = Chosen == 0? AType : BType;
  const ExtQuals *AExt = AType.getExtQualsPtrOnNull();
  const ExtQuals *BExt = BType.getExtQualsPtrOnNull();
  auto AK = AExt? AExt->getArithmeticKindSelector() : ExtQuals::DefaultArithmeticKind;
  auto BK = BExt? BExt->getArithmeticKindSelector() : ExtQuals::DefaultArithmeticKind;
  if(AK == BK ||
     (Chosen == 0 && AK >= BK) ||
     (Chosen != 0 && BK >= AK)) {
    if(Chosen == 0)
      B = ImplicitCast(C, ChosenType, B);
    else
      A = ImplicitCast(C, ChosenType, A);
    return ChosenType;
  }
  auto ReturnType = C.getQualTypeOtherKind(ChosenType, Chosen == 0? BType : AType);
  A = ImplicitCast(C, ReturnType, A);
  B = ImplicitCast(C, ReturnType, B);
  return ReturnType;
}


ExprResult Sema::TypecheckAssignment(QualType LHSTypeof, ExprResult RHS,
                                     SourceLocation Loc, SourceLocation MinLoc) {
  const Type *LHSType = LHSTypeof.getTypePtr();
  const Type *RHSType = RHS.get()->getType().getTypePtr();
  auto LHSExtQuals = LHSTypeof.getExtQualsPtrOnNull();
  auto RHSExtQuals = RHS.get()->getType().getExtQualsPtrOnNull();

  // Arithmetic assigment
  bool IsRHSInteger = RHSType->isIntegerType();
  bool IsRHSReal = RHSType->isRealType();
  bool IsRHSComplex = RHSType->isComplexType();
  bool IsRHSArithmetic = IsRHSInteger || IsRHSReal ||
                         IsRHSComplex;

  if(LHSType->isIntegerType()) {
    if(IsRHSInteger && ExtQualsSameKind(LHSExtQuals, RHSExtQuals)) ;
    else if(IsRHSArithmetic)
      RHS = ImplicitCastExpr::Create(Context, RHS.get()->getLocation(),
                                     LHSTypeof, RHS.take());
    else goto typeError;
  } else if(LHSType->isRealType()) {
    if(IsRHSReal && ExtQualsSameKind(LHSExtQuals, RHSExtQuals)) ;
    else if(IsRHSArithmetic)
      RHS = ImplicitCastExpr::Create(Context, RHS.get()->getLocation(),
                                     LHSTypeof, RHS.take());
    else goto typeError;
  } else if(LHSType->isComplexType()) {
    if(IsRHSComplex && ExtQualsSameKind(LHSExtQuals, RHSExtQuals)) ;
    else if(IsRHSArithmetic)
      RHS = ImplicitCastExpr::Create(Context, RHS.get()->getLocation(),
                                     LHSTypeof, RHS.take());
    else goto typeError;
  }

  // Logical assignment
  else if(LHSType->isLogicalType()) {
    if(!RHSType->isLogicalType()) goto typeError;
  }

  // Character assignment
  else if(LHSType->isCharacterType()) {
    if(!RHSType->isCharacterType()) goto typeError;
  }

  // Invalid assignment
  else goto typeError;

  return RHS;
typeError:
  Diags.Report(Loc,diag::err_typecheck_assign_incompatible)
      << LHSTypeof << RHS.get()->getType()
      << SourceRange(MinLoc,
                     RHS.get()->getLocEnd());
  return ExprError();
}


ExprResult Sema::ActOnComplexConstantExpr(ASTContext &C, SourceLocation Loc,
                                          SourceLocation MaxLoc,
                                          ExprResult RealPart, ExprResult ImPart) {
  APFloat Re = GetNumberConstant(Diags, RealPart.take()),
          Im = GetNumberConstant(Diags, ImPart.take());
  ComplexConstantExpr::Kind Kind = ComplexConstantExpr::Kind4;
  auto ReKind = llvm::APFloat::semanticsPrecision(Re.getSemantics());
  auto ImKind = llvm::APFloat::semanticsPrecision(Im.getSemantics());
  if(ReKind != ImKind) {
    // one is double, other is single
    bool IsExact;
    if(ReKind > ImKind)
      Im.convert(Re.getSemantics(), llvm::APFloat::rmNearestTiesToEven, &IsExact);
    else
      Re.convert(Im.getSemantics(), llvm::APFloat::rmNearestTiesToEven, &IsExact);
    Kind = ComplexConstantExpr::Kind8;
  } else {
    if(ReKind == llvm::APFloat::semanticsPrecision(llvm::APFloat::IEEEdouble))
       Kind = ComplexConstantExpr::Kind8;
  }
  return ComplexConstantExpr::Create(C, Loc,
                                     MaxLoc, Re, Im, Kind);
}

ExprResult Sema::ActOnUnaryExpr(ASTContext &C, SourceLocation Loc,
                                UnaryExpr::Operator Op, ExprResult E) {
  unsigned DiagType = 0;

  auto EType = E.get()->getType();

  switch(Op) {
  // Arithmetic unary expression
  case UnaryExpr::Plus: case UnaryExpr::Minus:
    if(GetArithmeticTypeSpec(EType) == TST_unspecified) {
      DiagType = diag::err_typecheck_arith_unary_expr;
      goto typecheckInvalidOperand;
    }
    break;

  // Logical unary expression
  case UnaryExpr::Not:
    if(!EType->isLogicalType()) {
      DiagType = diag::err_typecheck_logical_unary_expr;
      goto typecheckInvalidOperand;
    }
    break;

  default:
    llvm_unreachable("Unknown unary expression");
  }

  return UnaryExpr::Create(C, Loc, Op, E.take());

typecheckInvalidOperand:
  Diags.Report(Loc,DiagType)
      << EType
      << SourceRange(Loc,
                     E.get()->getLocEnd());
  return ExprError();
}

static bool Fortran77ArithmeticBinaryTypingRules(ASTContext &C,
                                                 BinaryExpr::Operator Op,
                                                 QualType &ReturnType,
                                                 ExprResult &LHS, ExprResult &RHS,
                                                 QualType LHSType, QualType RHSType) {
  auto LHSTypeSpec = GetArithmeticTypeSpecFortran77(LHSType);
  auto RHSTypeSpec = GetArithmeticTypeSpecFortran77(RHSType);
  ReturnType = LHSType;
  switch(RHSTypeSpec) {
  // I2:
  // I2: I/R/D/C = I1/R1/D1/C1 ** I2
  // OR
  // I = I1 <op> I2
  // R = R1 <op> REAL(I2)
  // D = D1 <op> DBLE(I2)
  // C = C1 <op> CMPLX(REAL(I2),0.0)
  case TST_integer:
    if(Op == BinaryExpr::Power)
      break;
    switch(LHSTypeSpec) {
    case TST_integer:
      break;
    case TST_real:
    case TST_doubleprecision:
    case TST_complex:
      RHS = ImplicitCast(C, ReturnType, RHS);
      break;
    default:
      llvm_unreachable("Unknown Arithmetic TST");
    }
    break;

  // R2:
  // R = REAL(I1) <op> R2
  // R = R1 <op> R2
  // D = D1 <op> DBLE(R2)
  // C = C1 <op> CMPLX(R2,0.0)
  case TST_real:
    switch(LHSTypeSpec) {
    case TST_integer:
      ReturnType = RHSType;
      LHS = ImplicitCast(C, ReturnType, LHS);
      break;
    case TST_real: break;
    case TST_doubleprecision: case TST_complex:
      RHS = ImplicitCast(C, ReturnType, RHS);
      break;
    default:
      llvm_unreachable("Unknown Arithmetic TST");
    }
    break;

  // D2:
  // D = DBLE(I1) <op> D2
  // D = DBLE(R1) <op> D2
  // D = D1 <op> D2
  // C1 - prohibited
  case TST_doubleprecision:
    switch(LHSTypeSpec) {
    case TST_integer: case TST_real:
      ReturnType = RHSType;
      LHS = ImplicitCast(C, ReturnType, LHS);
      break;
    case TST_doubleprecision: break;
    case TST_complex:
      goto typecheckInvalidOperands;
    default:
      llvm_unreachable("Unknown Arithmetic TST");
    }
    break;

  // C2:
  // C = CMPLX(REAL(I1),0.0) <op> C2
  // C = CMPLX(R1,0.0) <op> C2
  // D1 - prohibited
  // C = C1 <op> C2
  case TST_complex:
    switch(LHSTypeSpec) {
    case TST_integer: case TST_real:
      ReturnType = RHSType;
      LHS = ImplicitCast(C, ReturnType, LHS);
      break;
    case TST_doubleprecision:
      goto typecheckInvalidOperands;
    case TST_complex: break;
    default:
      llvm_unreachable("Unknown Arithmetic TST");
    }
    break;

  default:
    llvm_unreachable("Unknown Arithmetic TST");
  }

  return false;
typecheckInvalidOperands:
  return true;
}


// Kind selection rules:
// where typeof(x i) == Integer and typeof(x !i) : Real/Complex, Kind = Real/Complex
// where kindof(x i) == kindof(x !i), Kind = x i
// where typeof(x1 and x2) == Integer, Kind = largest kind
// where typeof(x1 and x2) == Real/Complex, Kind = largest kind

// Conversion matrix:
// LHS     RHS
//  I  | I, R, Z => I, R, Z
//  R  | I, R, Z => R, R, Z
//  Z  | I, R, Z => Z, Z, Z
//
// x1 ** x2 where x1 is real/complex and x2 is int, x2 not converted.
static void Fortran90ArithmeticBinaryTypingRules(ASTContext &C,
                                                 BinaryExpr::Operator Op,
                                                 QualType &ReturnType,
                                                 ExprResult &LHS, ExprResult &RHS,
                                                 QualType LHSType, QualType RHSType,
                                                 TypeSpecifierType LHSTypeSpec,
                                                 TypeSpecifierType RHSTypeSpec) {
  if(LHSTypeSpec == TST_integer) {
    if(RHSTypeSpec == TST_integer)
      ReturnType = SelectLargestKindApplyConversions(C, LHS, RHS, LHSType, RHSType);
    else {
      ReturnType = RHSType;
      LHS = ImplicitCast(C, ReturnType, LHS);
    }
  } else {
    // LHS is real/complex
    if(RHSTypeSpec == TST_integer) {
      ReturnType = LHSType;
      // no need for conversion when ** is used.
      if(Op != BinaryExpr::Power) RHS = ImplicitCast(C, ReturnType, RHS);
    }
    else if(LHSTypeSpec == TST_real) {
      if(RHSTypeSpec == TST_real)
        ReturnType = SelectLargestKindApplyConversions(C, LHS, RHS, LHSType, RHSType);
      else  // RHS is complex
        ReturnType = TakeTypeSelectLargestKindApplyConversion(C, 1, LHS, RHS,
                                                              LHSType, RHSType);
    }
    else if(LHSTypeSpec == TST_complex) {
      if(RHSTypeSpec == TST_complex)
        ReturnType = SelectLargestKindApplyConversions(C, LHS, RHS, LHSType, RHSType);
      else  // RHS is real
        ReturnType = TakeTypeSelectLargestKindApplyConversion(C, 0, LHS, RHS,
                                                              LHSType, RHSType);
    }
  }
}

// FIXME: verify return type for binary expressions.
ExprResult Sema::ActOnBinaryExpr(ASTContext &C, SourceLocation Loc,
                                 BinaryExpr::Operator Op,
                                 ExprResult LHS, ExprResult RHS) {
  unsigned DiagType = 0;

  auto LHSType = LHS.get()->getType();
  auto RHSType = RHS.get()->getType();
  QualType ReturnType;

  switch(Op) {
  // Arithmetic binary expression
  case BinaryExpr::Plus: case BinaryExpr::Minus:
  case BinaryExpr::Multiply: case BinaryExpr::Divide:
  case BinaryExpr::Power: {
    DiagType = diag::err_typecheck_arith_invalid_operands;

    auto LHSTypeSpec = GetArithmeticTypeSpec(LHSType);
    auto RHSTypeSpec = GetArithmeticTypeSpec(RHSType);

    if(LHSTypeSpec == TST_unspecified || RHSTypeSpec == TST_unspecified)
      goto typecheckInvalidOperands;

    if(Fortran77ArithmeticBinaryTypingRules(C, Op, ReturnType, LHS, RHS, LHSType, RHSType))
      goto typecheckInvalidOperands;

    break;
  }

  // Logical binary expression
  case BinaryExpr::And: case BinaryExpr::Or:
  case BinaryExpr::Eqv: case BinaryExpr::Neqv: {
    DiagType = diag::err_typecheck_logical_invalid_operands;

    if(!LHSType->isLogicalType()) goto typecheckInvalidOperands;
    if(!RHSType->isLogicalType()) goto typecheckInvalidOperands;
    ReturnType = C.LogicalTy;
    break;
  }

  // Character binary expression
  case BinaryExpr::Concat: {
    DiagType = diag::err_typecheck_char_invalid_operands;

    if(!LHSType->isCharacterType()) goto typecheckInvalidOperands;
    if(!RHSType->isCharacterType()) goto typecheckInvalidOperands;
    ReturnType = C.CharacterTy;
    break;
  }

  // relational binary expression
  case BinaryExpr::Equal: case BinaryExpr::NotEqual:
  case BinaryExpr::GreaterThan: case BinaryExpr::GreaterThanEqual:
  case BinaryExpr::LessThan: case BinaryExpr::LessThanEqual: {
    DiagType = diag::err_typecheck_relational_invalid_operands;
    ReturnType = C.LogicalTy;

    // Character relational expression
    if(LHSType->isCharacterType() && RHSType->isCharacterType()) break;

    // Arithmetic relational expression
    auto LHSTypeSpec = GetArithmeticTypeSpec(LHSType);
    auto RHSTypeSpec = GetArithmeticTypeSpec(RHSType);

    if(LHSTypeSpec == TST_unspecified || RHSTypeSpec == TST_unspecified)
      goto typecheckInvalidOperands;

    // A complex operand is permitted only when the relational operator is .EQ. or .NE.
    if((LHSTypeSpec == TST_complex ||
        RHSTypeSpec == TST_complex)) {
      if(Op != BinaryExpr::Equal && Op != BinaryExpr::NotEqual)
        goto typecheckInvalidOperands;

      // Fortran 77: The comparison of a double precision value and a complex value is not permitted.
      auto LHSTypeSpecF77 = GetArithmeticTypeSpecFortran77(LHSType);
      auto RHSTypeSpecF77 = GetArithmeticTypeSpecFortran77(RHSType);
      if(LHSTypeSpecF77 == TST_doubleprecision ||
         RHSTypeSpecF77 == TST_doubleprecision)
        goto typecheckInvalidOperands;
    }

    if(LHSTypeSpec == RHSTypeSpec) {
      // upcast to largest kind
      SelectLargestKindApplyConversions(C, LHS, RHS, LHSType, RHSType);
    } else {
      if(LHSTypeSpec == TST_integer)
        // RHS is real/complex
        LHS = ImplicitCast(C, RHSType, LHS);
      else if(LHSTypeSpec == TST_real) {
        if(RHSTypeSpec == TST_integer)
          RHS = ImplicitCast(C, LHSType, RHS);
        else LHS = ImplicitCast(C, RHSType, LHS);
      } else {
        // lhs is complex
        // rhs is int/real
        RHS = ImplicitCast(C, LHSType, RHS);
      }
    }

    break;
  }

  default:
    llvm_unreachable("Unknown binary expression");
  }

  return BinaryExpr::Create(C, Loc, Op, ReturnType, LHS.take(), RHS.take());

typecheckInvalidOperands:
  Diags.Report(Loc,DiagType)
      << LHSType << RHSType
      << SourceRange(LHS.get()->getLocStart(),
                     RHS.get()->getLocEnd());
  return ExprError();
}

static bool IsIntegerExpression(ExprResult E) {
  return E.get()->getType()->isIntegerType();
}

ExprResult Sema::ActOnSubstringExpr(ASTContext &C, SourceLocation Loc, ExprResult Target,
                                    ExprResult StartingPoint, ExprResult EndPoint) {
  bool HasErrors = false;
  if(StartingPoint.get() && !IsIntegerExpression(StartingPoint.get())) {
    Diags.Report(StartingPoint.get()->getLocation(),
                 diag::err_expected_integer_expr)
      << StartingPoint.get()->getSourceRange();
    HasErrors = true;
  }
  if(EndPoint.get() && !IsIntegerExpression(EndPoint.get())) {
    Diags.Report(EndPoint.get()->getLocation(),
                 diag::err_expected_integer_expr)
      << EndPoint.get()->getSourceRange();
    HasErrors = true;
  }
  if(HasErrors) return ExprError();

  return SubstringExpr::Create(C, Loc, Target.take(),
                               StartingPoint.take(), EndPoint.take());
}

bool Sema::CheckSubscriptExprDimensionCount(SourceLocation Loc,
                                            ExprResult Target,
                                            ArrayRef<ExprResult> Arguments) {
  auto AT = Target.get()->getType().getTypePtr()->asArrayType();
  assert(AT);
  if(AT->getDimensionCount() != Arguments.size()) {
    Diags.Report(Loc,
                 diag::err_array_subscript_dimension_count_mismatch)
      << int(AT->getDimensionCount())
      << SourceRange(Loc, Arguments.back().get()->getLocEnd());
    return false;
  }
  return true;
}

ExprResult Sema::ActOnSubscriptExpr(ASTContext &C, SourceLocation Loc, ExprResult Target,
                                    llvm::ArrayRef<ExprResult> Subscripts) {
  assert(Subscripts.size());
  if(!CheckSubscriptExprDimensionCount(Loc, Target, Subscripts))
    return ExprError();

  llvm::SmallVector<Expr*, 8> Subs(Subscripts.size());
  //FIXME constraint
  //A subscript expression may contain array element references and function references.

  bool HasErrors = false;
  for(size_t I = 0; I < Subscripts.size(); ++I) {
    if(IsIntegerExpression(Subscripts[I]))
      Subs[I] = Subscripts[I].take();
    else {
      Diags.Report(Subscripts[I].get()->getLocation(),
                   diag::err_expected_integer_expr)
        << Subscripts[I].get()->getSourceRange();
      HasErrors = true;
    }
  }
  if(HasErrors) return ExprError();

  return ArrayElementExpr::Create(C, Loc, Target.take(), Subs);
}

ExprResult Sema::ActOnIntrinsicFunctionCallExpr(ASTContext &C, SourceLocation Loc,
                                                const IntrinsicFunctionDecl *FunctionDecl,
                                                ArrayRef<ExprResult> Arguments) {
  using namespace intrinsic;

  SmallVector<Expr*, 8> Args(Arguments.size());
  for(size_t I = 0; I < Arguments.size(); ++I)
    Args[I] = Arguments[I].take();

  auto Function = FunctionDecl->getFunction();

  // Check argument count
  unsigned ArgCountDiag = 0;
  int ExpectedCount = 0;
  const char *ExpectedString = nullptr;

  switch(getFunctionArgumentCount(Function)) {
  case ArgumentCount1:
    ExpectedCount = 1;
    if(Args.size() < 1)
      ArgCountDiag = diag::err_typecheck_call_too_few_args;
    else if(Args.size() > 1)
      ArgCountDiag = diag::err_typecheck_call_too_many_args;
    break;
  case ArgumentCount2:
    ExpectedCount = 2;
    if(Args.size() < 2)
      ArgCountDiag = diag::err_typecheck_call_too_few_args;
    else if(Args.size() > 2)
      ArgCountDiag = diag::err_typecheck_call_too_many_args;
    break;
  case ArgumentCount1or2:
    ExpectedString = "1 or 2";
    if(Args.size() < 1)
      ArgCountDiag = diag::err_typecheck_call_too_few_args;
    else if(Args.size() > 2)
      ArgCountDiag = diag::err_typecheck_call_too_many_args;
    break;
  case ArgumentCount2orMore:
    ExpectedCount = 2;
    if(Args.size() < 2)
      ArgCountDiag = diag::err_typecheck_call_too_few_args_at_least;
    break;
  default:
    llvm_unreachable("invalid arg count");
  }
  if(ArgCountDiag) {
    auto Reporter = Diags.Report(Loc, ArgCountDiag)
                      << /*intrinsic function=*/ 0;
    if(ExpectedString)
      Reporter << ExpectedString;
    else
      Reporter << ExpectedCount;
    Reporter << unsigned(Args.size());

    return ExprError();
  }

  // FIXME: TODO Check that all arguments are the same type
  if(Args.size() > 1) {
    for(size_t I = 1; I < Args.size(); ++I) {

    }
  }

  // Per function type checks.
  QualType ReturnType;
  auto FirstArgArithSpec = GetArithmeticTypeSpecFortran77(Args[0]->getType());
  auto FirstArgLoc = Args[0]->getLocation();
  auto FirstArgSourceRange = Args[0]->getSourceRange();

  switch(Function) {
  case INT: case IFIX: case IDINT:
    if(Function == IFIX && FirstArgArithSpec != TST_real) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.RealTy << FirstArgSourceRange;
    }
    else if(Function == IDINT && FirstArgArithSpec != TST_doubleprecision) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.DoublePrecisionTy << FirstArgSourceRange;
    }
    else if(FirstArgArithSpec == TST_unspecified) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << "'INTEGER' or 'REAL' or 'COMPLEX'"
        << FirstArgSourceRange;
    }

    ReturnType = C.IntegerTy;
    break;

  case REAL: case FLOAT: case SNGL:
    if(Function == FLOAT && FirstArgArithSpec != TST_integer) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.IntegerTy << FirstArgSourceRange;
    }
    else if(Function == SNGL && FirstArgArithSpec != TST_doubleprecision) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.DoublePrecisionTy << FirstArgSourceRange;
    }
    else if(FirstArgArithSpec == TST_unspecified) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << "'INTEGER' or 'REAL' or 'COMPLEX'"
        << FirstArgSourceRange;
    }

    ReturnType = C.RealTy;
    break;

  case DBLE:
    if(FirstArgArithSpec == TST_unspecified) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << "'INTEGER' or 'REAL' or 'COMPLEX'"
        << FirstArgSourceRange;
    }

    ReturnType = C.DoublePrecisionTy;
    break;

  case CMPLX:
    if(FirstArgArithSpec == TST_unspecified) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << "'INTEGER' or 'REAL' or 'COMPLEX'"
        << FirstArgSourceRange;
    }

    ReturnType = C.ComplexTy;
    break;

  case ICHAR:
    if(!Args[0]->getType()->isCharacterType()) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.CharacterTy << FirstArgSourceRange;
    }
    ReturnType = C.IntegerTy;
    break;

  case CHAR:
    if(FirstArgArithSpec != TST_integer) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.IntegerTy << FirstArgSourceRange;
    }
    ReturnType = C.CharacterTy;
    break;

#define CASE_REAL_DOUBLE_OVERLOAD(NAME) \
  case NAME: \
    if(FirstArgArithSpec != TST_real && \
       FirstArgArithSpec != TST_doubleprecision) { \
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible) \
        << Args[0]->getType() << C.RealTy << FirstArgSourceRange; \
    }

#define CASE_DOUBLE_OVERLOAD(NAME) \
  case NAME: \
    if(FirstArgArithSpec != TST_doubleprecision) { \
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible) \
        << Args[0]->getType() << C.DoublePrecisionTy << FirstArgSourceRange; \
    }

#define CASE_COMPLEX_OVERLOAD(NAME) \
  case NAME: \
    if(FirstArgArithSpec != TST_complex) { \
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible) \
        << Args[0]->getType() << C.ComplexTy << FirstArgSourceRange; \
    }

  CASE_REAL_DOUBLE_OVERLOAD(NINT)
    ReturnType = C.IntegerTy;
    break;
  CASE_DOUBLE_OVERLOAD(IDNINT)
    ReturnType = C.IntegerTy;
    break;

  case ABS:
    if(FirstArgArithSpec == TST_unspecified) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << "'INTEGER' or 'REAL' or 'COMPLEX'"
        << FirstArgSourceRange;
    }
    if(FirstArgArithSpec == TST_complex)
      ReturnType = C.RealTy;
    else
      ReturnType = Args[0]->getType();
    break;
  CASE_COMPLEX_OVERLOAD(CABS)
    ReturnType = C.RealTy;
    break;

  case LEN:
    if(!Args[0]->getType()->isCharacterType()) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.CharacterTy << FirstArgSourceRange;
    }
    ReturnType = C.IntegerTy;
    break;
  case INDEX:
    if(!Args[0]->getType()->isCharacterType()) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.CharacterTy << FirstArgSourceRange;
    }
    ReturnType = C.IntegerTy;
    break;

  CASE_COMPLEX_OVERLOAD(AIMAG)
    ReturnType = C.RealTy;
    break;

  // FIXME: add the rest.

  // Real + double + complex
  case SQRT: case EXP: case LOG:
  case SIN: case COS:
    if(FirstArgArithSpec == TST_unspecified ||
       FirstArgArithSpec == TST_integer) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << "'REAL' or 'COMPLEX'"
        << FirstArgSourceRange;
    }
    else ReturnType = Args[0]->getType();
    break;

  // Real + double
  case ATAN2:
    //if(!Args[1]->getType()->isRealType() &&
    //   !Args[1]->getType()->isDoublePrecisionType()) {
    //  Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
    //    << Args[1]->getType() << C.RealTy
    //    << Args[1]->getSourceRange();
    //}
  case AINT: case ANINT:
  case LOG10: case TAN: case ASIN:
  case ACOS: case ATAN:
  case SINH: case COSH: case TANH:
    if(FirstArgArithSpec != TST_real &&
       FirstArgArithSpec != TST_doubleprecision) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.RealTy << FirstArgSourceRange;
    }
    else ReturnType = Args[0]->getType();
    break;

  // int
  case IABS:
    if(FirstArgArithSpec != TST_integer) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.IntegerTy << FirstArgSourceRange;
    }
    else ReturnType = Args[0]->getType();
    break;

  // real
  case ALOG: case ALOG10:
    if(FirstArgArithSpec != TST_real) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.RealTy << FirstArgSourceRange;
    }
    else ReturnType = Args[0]->getType();
    break;

  // double
  case DATAN2:
    //if(!Args[1]->getType()->isDoublePrecisionType()) {
    //  Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
    //    << Args[1]->getType() << C.DoublePrecisionTy
    //    << Args[1]->getSourceRange();
    //}
  case DINT: case DNINT:
  case DABS:
  case DSQRT: case DEXP: case DLOG:
  case DLOG10: case DSIN: case DCOS:
  case DTAN: case DASIN: case DACOS:
  case DATAN: case DSINH:
  case DCOSH: case DTANH:
    if(FirstArgArithSpec != TST_doubleprecision) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.DoublePrecisionTy << FirstArgSourceRange;
    }
    else ReturnType = Args[0]->getType();
    break;

  // complex
  case CONJG:
  case CSQRT: case CEXP: case CLOG:
  case CSIN: case CCOS:
    if(FirstArgArithSpec != TST_complex) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.ComplexTy << FirstArgSourceRange;
    }
    else ReturnType = Args[0]->getType();
    break;

  case LGE: case LGT: case LLE: case LLT:
    if(!Args[0]->getType()->isCharacterType()) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[0]->getType() << C.CharacterTy << FirstArgSourceRange;
    }
    if(!Args[1]->getType()->isCharacterType()) {
      Diags.Report(FirstArgLoc, diag::err_typecheck_passing_incompatible)
        << Args[1]->getType() << C.CharacterTy << FirstArgSourceRange;
    }
    ReturnType = C.LogicalTy;
    break;
  }

  if(ReturnType.isNull()) {
    ReturnType = C.RealTy; //An error occurred.
  }

  return IntrinsicFunctionCallExpr::Create(C, Loc, Function,
                                           Args, ReturnType);
}

} // namespace flang
