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

namespace flang {


/// Returns TST_integer/TST_real/TST_complex if a given type
/// is an arithmetic type, or TST_unspecified otherwise
static TypeSpecifierType GetArithmeticTypeSpec(QualType T) {
  if(T->isIntegerType()) return TST_integer;
  else if(T->isRealType()) return TST_real;
  else if(T->isComplexType()) return TST_complex;
  else return TST_unspecified;
}

/// Returns true if a real type is also a double precision type (Kind is 8).
static bool IsRealTypeDoublePrecision(QualType T) {
  auto Ext = T.getExtQualsPtrOrNull();
  return Ext && Ext->getKindSelector() == BuiltinType::Real8? true : false;
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
    return !IsRealTypeDoublePrecision(T);
  return false;
}

/// Returns true if a type is a double precision complex type (Kind is 8).
static bool IsTypeDoublePrecisionComplex(QualType T) {
  auto Ext = T.getExtQualsPtrOrNull();
  return T->isComplexType() &&
           Ext && Ext->getKindSelector() == BuiltinType::Real8? true : false;
}

static llvm::APFloat GetRealConstant(ASTContext &C, DiagnosticsEngine &Diags,
                                     Expr *E, const llvm::fltSemantics &FPType) {
  if(auto Real = dyn_cast<RealConstantExpr>(E)) {
    bool IsExact;
    auto Value = Real->getValue();
    Value.convert(FPType,
                  llvm::APFloat::rmNearestTiesToEven,
                  &IsExact);
    return Value;
  } else if(auto Int = dyn_cast<IntegerConstantExpr>(E)) {
    llvm::APFloat result(FPType);
    result.convertFromAPInt(Int->getValue(), true, llvm::APFloat::rmNearestTiesToEven);
    return result;
  } else if(auto Unary = dyn_cast<UnaryExpr>(E)) {
    auto Value = GetRealConstant(C, Diags, Unary->getExpression(),FPType);
    if(Unary->getOperator() == UnaryExpr::Minus)
      Value.changeSign();
    return Value;
  } else {
    Diags.Report(E->getLocation(), diag::err_expected_integer_or_real_constant_expr)
     << E->getSourceRange();
    return llvm::APFloat(FPType,"0");
  }
}

/// Returns true if two arithmetic type qualifiers have the same kind
static bool ExtQualsSameKind(const ASTContext &C,
                             const ExtQuals *A, const ExtQuals *B,
                             QualType AT, QualType BT) {
  return C.ArithmeticTypesSameKind(A, AT, B, BT);
}

/// Returns the largest kind between two arithmetic type qualifiers.
static int GetLargestKind(const ASTContext &C,
                          const ExtQuals *A, const ExtQuals *B,
                          QualType AT, QualType BT) {
  auto KindA = C.getArithmeticTypeKind(A, AT);
  auto KindB = C.getArithmeticTypeKind(B, BT);
  return C.getTypeKindBitWidth(KindA) >= C.getTypeKindBitWidth(KindB)? 0 : 1;
}

/// Creates an implicit cast expression
static Expr *ImplicitCast(ASTContext &C, QualType T, ExprResult E) {
  return ImplicitCastExpr::Create(C, E.get()->getLocation(), T, E.take());
}

/// Selects the type with the biggest kind from two arithmetic types,
/// applies any required conversions to that type for two expressions,
/// and returns that type.
/// NB: This assumes that type A and type B have the same base type,
/// i.e. Int and Int
static QualType SelectLargestKindApplyConversions(ASTContext &C,
                                                  ExprResult &A, ExprResult &B,
                                                  QualType AType, QualType BType) {
  auto AK = C.getArithmeticTypeKind(AType.getExtQualsPtrOrNull(), AType);
  auto BK = C.getArithmeticTypeKind(BType.getExtQualsPtrOrNull(), BType);

  if(AK == BK) return AType;
  else if(C.getTypeKindBitWidth(AK) >=
          C.getTypeKindBitWidth(BK)) {
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
  const ExtQuals *AExt = AType.getExtQualsPtrOrNull();
  const ExtQuals *BExt = BType.getExtQualsPtrOrNull();
  auto AK = C.getArithmeticTypeKind(AExt, AType);
  auto BK = C.getArithmeticTypeKind(BExt, BType);
  auto AKWidth = C.getTypeKindBitWidth(AK);
  auto BKWidth = C.getTypeKindBitWidth(BK);
  if(AK == BK ||
     (Chosen == 0 && AKWidth >= BKWidth) ||
     (Chosen != 0 && BKWidth >= AKWidth)) {
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

static QualType TypeWithKind(ASTContext &C, QualType T, QualType TKind) {
  const ExtQuals *AExt = T.getExtQualsPtrOrNull();
  const ExtQuals *BExt = TKind.getExtQualsPtrOrNull();
  auto AK = C.getArithmeticTypeKind(AExt, T);
  auto BK = C.getArithmeticTypeKind(BExt, TKind);
  if(AK == BK) return T;
  return C.getQualTypeOtherKind(T, TKind);
}

ExprResult Sema::TypecheckAssignment(QualType LHSTypeof, ExprResult RHS,
                                     SourceLocation Loc, SourceLocation MinLoc,
                                     SourceRange ExtraRange) {
  const Type *LHSType = LHSTypeof.getTypePtr();
  auto RHSTypeof = RHS.get()->getType();
  const Type *RHSType = RHSTypeof.getTypePtr();
  auto LHSExtQuals = LHSTypeof.getExtQualsPtrOrNull();
  auto RHSExtQuals = RHS.get()->getType().getExtQualsPtrOrNull();

  // Arithmetic assigment
  bool IsRHSInteger = RHSType->isIntegerType();
  bool IsRHSReal = RHSType->isRealType();
  bool IsRHSComplex = RHSType->isComplexType();
  bool IsRHSArithmetic = IsRHSInteger || IsRHSReal ||
                         IsRHSComplex;

  if(LHSType->isIntegerType()) {
    if(IsRHSInteger && ExtQualsSameKind(Context, LHSExtQuals, RHSExtQuals,
                                        LHSTypeof, RHSTypeof)) ;
    else if(IsRHSArithmetic)
      RHS = ImplicitCastExpr::Create(Context, RHS.get()->getLocation(),
                                     LHSTypeof, RHS.take());
    else goto typeError;
  } else if(LHSType->isRealType()) {
    if(IsRHSReal && ExtQualsSameKind(Context, LHSExtQuals, RHSExtQuals,
                                     LHSTypeof, RHSTypeof)) ;
    else if(IsRHSArithmetic)
      RHS = ImplicitCastExpr::Create(Context, RHS.get()->getLocation(),
                                     LHSTypeof, RHS.take());
    else goto typeError;
  } else if(LHSType->isComplexType()) {
    if(IsRHSComplex && ExtQualsSameKind(Context, LHSExtQuals, RHSExtQuals,
                                        LHSTypeof, RHSTypeof)) ;
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
  auto Reporter = Diags.Report(Loc,diag::err_typecheck_assign_incompatible)
      << LHSTypeof << RHS.get()->getType()
      << SourceRange(MinLoc,
                     RHS.get()->getLocEnd());
  if(ExtraRange.isValid())
    Reporter << ExtraRange;
  return ExprError();
}


ExprResult Sema::ActOnComplexConstantExpr(ASTContext &C, SourceLocation Loc,
                                          SourceLocation MaxLoc,
                                          ExprResult RealPart, ExprResult ImPart) {
  QualType T1 = RealPart.get()->getType();
  QualType T2 = ImPart.get()->getType();
  QualType ElementType = T1;

  if(T1->isRealType() || T2->isRealType()) {
    if(T1->isRealType() && T2->isRealType()) {
      auto ReKind = llvm::APFloat::semanticsPrecision(C.getFPTypeSemantics(T1));
      auto ImKind = llvm::APFloat::semanticsPrecision(C.getFPTypeSemantics(T2));
      if(ReKind < ImKind)
        ElementType = T2;
    }
    else if(T2->isRealType()) ElementType = T2;
  } else ElementType = C.RealTy;

  const llvm::fltSemantics& ElementSemantics = C.getFPTypeSemantics(ElementType);

  APFloat Re = GetRealConstant(C, Diags, RealPart.take(), ElementSemantics),
          Im = GetRealConstant(C, Diags, ImPart.take(), ElementSemantics);

  return ComplexConstantExpr::Create(C, Loc,
                                     MaxLoc, Re, Im,
                                     C.getComplexType(ElementType));
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

    // Fortran 77: Disallow operations between double precision and complex
    if((LHSTypeSpec == TST_complex ||
        RHSTypeSpec == TST_complex)) {
      if((IsTypeDoublePrecisionReal(LHSType) && !IsTypeDoublePrecisionComplex(RHSType)) ||
         (IsTypeDoublePrecisionReal(RHSType) && !IsTypeDoublePrecisionComplex(LHSType)))
        goto typecheckInvalidOperands;
    }

    Fortran90ArithmeticBinaryTypingRules(C, Op, ReturnType, LHS, RHS,
                                         LHSType, RHSType, LHSTypeSpec, RHSTypeSpec);

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
      if(IsTypeDoublePrecisionReal(LHSType) ||
         IsTypeDoublePrecisionReal(RHSType))
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

bool Sema::CheckCallArgumentCount(FunctionDecl *Function, ArrayRef<Expr*> Arguments, SourceLocation Loc,
                                  SourceRange FuncNameRange) {
  if(Function->isExternal()) {
    if(Arguments.empty()) return true;
    if(Function->getArguments().empty()) {
      // Infer external function arguments.
      llvm::SmallVector<VarDecl*, 8> Args(Arguments.size());
      for(size_t I = 0; I < Arguments.size(); ++I) {
        auto Arg = VarDecl::CreateArgument(Context, Function,
                                           Function->getLocation(), Function->getIdentifier());
        Arg->setType(Arguments[I]->getType());
        Args[I] = Arg;
      }
      Function->setArguments(Context, Args);
    }
  }

  // check the arguments.
  // NB: Highlight as clang does:
  // Too few args, range with function name, loc - ')' location
  // Too many args, range with function name, loc and range - extra arguments.
  auto FunctionArgs = Function->getArguments();
  if(Arguments.size() != FunctionArgs.size()) {
    unsigned FuncType = /*function=*/ (Function->isSubroutine()? 2 : 1);
    if(Arguments.size() < FunctionArgs.size()) {
      Diags.Report(Loc, diag::err_typecheck_call_too_few_args)
          << FuncType << unsigned(FunctionArgs.size())
          << unsigned(Arguments.size())
          << FuncNameRange;
    } else {
      auto LocStart = Arguments[FunctionArgs.size()]->getLocStart();
      auto LocEnd   = Arguments.back()->getLocEnd();
      Diags.Report(LocStart, diag::err_typecheck_call_too_many_args)
          << FuncType << unsigned(FunctionArgs.size())
          << unsigned(Arguments.size())
          << FuncNameRange << SourceRange(LocStart, LocEnd);
    }
    return false;
  }

  return true;
}

ExprResult Sema::ActOnCallExpr(ASTContext &C, SourceLocation Loc, SourceLocation RParenLoc,
                               SourceRange IdRange,
                               FunctionDecl *Function, ArrayRef<ExprResult> Arguments) {
  assert(!Function->isSubroutine());

  SmallVector<Expr*, 8> Args(Arguments.size());
  for(size_t I = 0; I < Arguments.size(); ++I)
    Args[I] = Arguments[I].take();

  CheckCallArgumentCount(Function, Args, RParenLoc, IdRange);

  return CallExpr::Create(C, Loc, Function, Args);
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
  if(CheckIntrinsicCallArgumentCount(Function, Args, Loc))
    return ExprError();

  // Per function type checks.
  QualType ReturnType;
  switch(getFunctionGroup(Function)) {
  case GROUP_CONVERSION:
    CheckIntrinsicConversionFunc(Function, Args, ReturnType);
    break;

  case GROUP_TRUNCATION:
    CheckIntrinsicTruncationFunc(Function, Args, ReturnType);
    break;

  case GROUP_COMPLEX:
    CheckIntrinsicComplexFunc(Function, Args, ReturnType);
    break;

  case GROUP_MATHS:
    CheckIntrinsicMathsFunc(Function, Args, ReturnType);
    break;

  case GROUP_CHARACTER:
    CheckIntrinsicCharacterFunc(Function, Args, ReturnType);
    break;

  default:
    llvm_unreachable("invalid intrinsic function");
  }

  if(ReturnType.isNull())
    ReturnType = C.RealTy; //An error occurred.

  return IntrinsicCallExpr::Create(C, Loc, Function,
                                   Args, ReturnType);
}

} // namespace flang
