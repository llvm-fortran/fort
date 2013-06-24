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

static TypeSpecifierType GetArithmeticTypeSpec(const Type *T) {
  if(T->isIntegerType()) return TST_integer;
  else if(T->isRealType()) return TST_real;
  else if(T->isDoublePrecisionType()) return TST_doubleprecision;
  else if(T->isComplexType()) return TST_complex;
  else return TST_unspecified;
}

ExprResult Sema::ActOnUnaryExpr(ASTContext &C, SourceLocation Loc,
                                UnaryExpr::Operator Op, ExprResult E) {
  unsigned DiagType = 0;

  auto EType = E.get()->getType().getTypePtr();

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
      << E.get()->getType()
      << SourceRange(Loc,
                     E.get()->getLocEnd());
  return ExprError();
}

// FIXME: verify return type for binary expressions.
ExprResult Sema::ActOnBinaryExpr(ASTContext &C, SourceLocation Loc,
                                 BinaryExpr::Operator Op,
                                 ExprResult LHS, ExprResult RHS) {
  unsigned DiagType = 0;

  auto LHSType = LHS.get()->getType().getTypePtr();
  auto RHSType = RHS.get()->getType().getTypePtr();

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

    switch(RHSTypeSpec) {
    // I2:
    // I2: I/R/D/C = I1/R1/D1/C1 ** I2
    // OR
    // I = I1 <op> I2
    // R = R1 <op> REAL(I2)
    // D = D1 <op> DBLE(I2)
    // C = C1 <op> CMPLX(REAL(I2),0.0)
    case TST_integer:
      if(Op == BinaryExpr::Power) break;
      switch(LHSTypeSpec) {
      case TST_integer: break;
      case TST_real:
        RHS = ImplicitCastExpr::Create(C, RHS.get()->getLocation(),
                                       intrinsic::REAL, RHS.take());
        break;
      case TST_doubleprecision:
        RHS = ImplicitCastExpr::Create(C, RHS.get()->getLocation(),
                                       intrinsic::DBLE, RHS.take());
        break;
      case TST_complex:
        RHS = ImplicitCastExpr::Create(C, RHS.get()->getLocation(),
                                       intrinsic::CMPLX, RHS.take());
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
        LHS = ImplicitCastExpr::Create(C, LHS.get()->getLocation(),
                                       intrinsic::REAL, LHS.take());
        break;
      case TST_real: break;
      case TST_doubleprecision:
        RHS = ImplicitCastExpr::Create(C, RHS.get()->getLocation(),
                                       intrinsic::DBLE, RHS.take());
        break;
      case TST_complex:
        RHS = ImplicitCastExpr::Create(C, RHS.get()->getLocation(),
                                       intrinsic::CMPLX, RHS.take());
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
        LHS = ImplicitCastExpr::Create(C, LHS.get()->getLocation(),
                                       intrinsic::DBLE, LHS.take());
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
        LHS = ImplicitCastExpr::Create(C, LHS.get()->getLocation(),
                                       intrinsic::CMPLX, LHS.take());
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
    break;
  }

  // Logical binary expression
  case BinaryExpr::And: case BinaryExpr::Or:
  case BinaryExpr::Eqv: case BinaryExpr::Neqv: {
    DiagType = diag::err_typecheck_logical_invalid_operands;

    if(!LHSType->isLogicalType()) goto typecheckInvalidOperands;
    if(!RHSType->isLogicalType()) goto typecheckInvalidOperands;
    break;
  }

  // Character binary expression
  case BinaryExpr::Concat: {
    DiagType = diag::err_typecheck_char_invalid_operands;

    if(!LHSType->isCharacterType()) goto typecheckInvalidOperands;
    if(!RHSType->isCharacterType()) goto typecheckInvalidOperands;
    break;
  }

  // relational binary expression
  case BinaryExpr::Equal: case BinaryExpr::NotEqual:
  case BinaryExpr::GreaterThan: case BinaryExpr::GreaterThanEqual:
  case BinaryExpr::LessThan: case BinaryExpr::LessThanEqual: {
    DiagType = diag::err_typecheck_relational_invalid_operands;

    // Character relational expression
    if(LHSType->isCharacterType() && RHSType->isCharacterType()) break;

    // Arithmetic relational expression
    auto LHSTypeSpec = GetArithmeticTypeSpec(LHSType);
    auto RHSTypeSpec = GetArithmeticTypeSpec(RHSType);

    if(LHSTypeSpec == TST_unspecified || RHSTypeSpec == TST_unspecified)
      goto typecheckInvalidOperands;

    // A complex operand is permitted only when the relational operator is .EQ. or .NE.
    // The comparison of a double precision value and a complex value is not permitted.
    if((LHSTypeSpec == TST_complex ||
        RHSTypeSpec == TST_complex)) {
      if(Op != BinaryExpr::Equal && Op != BinaryExpr::NotEqual)
        goto typecheckInvalidOperands;
      if(LHSTypeSpec == TST_doubleprecision ||
         RHSTypeSpec == TST_doubleprecision)
        goto typecheckInvalidOperands;
    }

    // typeof(e1) != typeof(e1) =>
    //   e1 <relop> e2 = ((e1) - (e2)) <relop> 0
    if(LHSTypeSpec != RHSTypeSpec) {
      LHS = ActOnBinaryExpr(C, Loc, BinaryExpr::Minus, LHS, RHS);
      switch(GetArithmeticTypeSpec(LHS.get()->getType().getTypePtr())) {
      case TST_integer:
        RHS = IntegerConstantExpr::Create(C, Loc, Loc, "0"); break;
      case TST_real:
        RHS = RealConstantExpr::Create(C, Loc, Loc, "0"); break;
      case TST_doubleprecision:
        RHS = DoublePrecisionConstantExpr::Create(C, Loc, Loc,"0"); break;
      case TST_complex:
        RHS = ComplexConstantExpr::Create(C, Loc, Loc, llvm::APFloat(0.0), llvm::APFloat(0.0));
        break;
      default:
        llvm_unreachable("Unknown Arithmetic TST");
      }
    }
    break;
  }

  default:
    llvm_unreachable("Unknown binary expression");
  }

  return BinaryExpr::Create(C, Loc, Op, LHS.take(), RHS.take());

typecheckInvalidOperands:
  Diags.Report(Loc,DiagType)
      << LHS.get()->getType() << RHS.get()->getType()
      << SourceRange(LHS.get()->getLocStart(),
                     RHS.get()->getLocEnd());
  return ExprError();
}

static bool IsIntegerExpression(ExprResult E) {
  return E.get()->getType().getTypePtr()->isIntegerType();
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

ExprResult Sema::ActOnSubscriptExpr(ASTContext &C, SourceLocation Loc, ExprResult Target,
                                    llvm::ArrayRef<ExprResult> Subscripts) {
  assert(Subscripts.size());
  auto AT = Target.get()->getType().getTypePtr()->asArrayType();
  assert(AT);
  if(AT->getDimensionCount() != Subscripts.size()) {
    Diags.Report(Loc,
                 diag::err_array_subscript_dimension_count_mismatch)
      << int(AT->getDimensionCount())
      << SourceRange(Loc, Subscripts.back().get()->getLocEnd());
    return ExprError();
  }
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
  auto FirstArgArithSpec = GetArithmeticTypeSpec(Args[0]->getType().getTypePtr());
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

  // FIXME: the rest
  case ABS:
    if(FirstArgArithSpec == TST_unspecified) {

    }
    ReturnType = Args[0]->getType();
    break;
  }

  return IntrinsicFunctionCallExpr::Create(C, Loc, Function,
                                           Args, ReturnType);
}

} // namespace flang
