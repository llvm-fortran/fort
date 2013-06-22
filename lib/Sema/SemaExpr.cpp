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

ExprResult Sema::ActOnUnaryExpr(ASTContext &C, llvm::SMLoc Loc,
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

  return UnaryExpr::Create(C, Loc, Op, E);

typecheckInvalidOperand:
  std::string TypeString;
  llvm::raw_string_ostream Stream(TypeString);
  E.get()->getType().print(Stream);
  Diags.Report(Loc,DiagType)
      << Stream.str()
      << llvm::SMRange(Loc,
                       E.get()->getMaxLocation());
  return ExprError();
}

// FIXME: verify return type for binary expressions.
ExprResult Sema::ActOnBinaryExpr(ASTContext &C, llvm::SMLoc Loc,
                                 BinaryExpr::Operator Op,
                                 ExprResult LHS,ExprResult RHS) {
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
        RHS = IntrinsicCallExpr::Create(C, RHS.get()->getLocation(),
                                     IntrinsicCallExpr::REAL,RHS);
        break;
      case TST_doubleprecision:
        RHS = IntrinsicCallExpr::Create(C, RHS.get()->getLocation(),
                                     IntrinsicCallExpr::DBLE,RHS);
        break;
      case TST_complex:
        RHS = IntrinsicCallExpr::Create(C, RHS.get()->getLocation(),
                                     IntrinsicCallExpr::CMPLX,RHS);
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
        LHS = IntrinsicCallExpr::Create(C, LHS.get()->getLocation(),
                                     IntrinsicCallExpr::REAL,LHS);
        break;
      case TST_real: break;
      case TST_doubleprecision:
        RHS = IntrinsicCallExpr::Create(C, RHS.get()->getLocation(),
                                     IntrinsicCallExpr::DBLE,RHS);
        break;
      case TST_complex:
        RHS = IntrinsicCallExpr::Create(C, RHS.get()->getLocation(),
                                     IntrinsicCallExpr::CMPLX,RHS);
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
        LHS = IntrinsicCallExpr::Create(C, LHS.get()->getLocation(),
                                     IntrinsicCallExpr::DBLE,LHS);
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
        LHS = IntrinsicCallExpr::Create(C, LHS.get()->getLocation(),
                                     IntrinsicCallExpr::CMPLX,LHS);
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
      case TST_integer: RHS = IntegerConstantExpr::Create(C, Loc, Loc, "0"); break;
      case TST_real: RHS = RealConstantExpr::Create(C, Loc, Loc, "0"); break;
      case TST_doubleprecision: RHS = DoublePrecisionConstantExpr::Create(C, Loc, Loc,"0"); break;
      case TST_complex: RHS = ComplexConstantExpr::Create(C, Loc, Loc, llvm::APFloat(0.0), llvm::APFloat(0.0)); break;
      default:
        llvm_unreachable("Unknown Arithmetic TST");
      }
    }
    break;
  }

  default:
    llvm_unreachable("Unknown binary expression");
  }

  return BinaryExpr::Create(C, Loc, Op, LHS, RHS);

typecheckInvalidOperands:
  std::string TypeStrings[2];
  llvm::raw_string_ostream StreamLHS(TypeStrings[0]),
      StreamRHS(TypeStrings[1]);
  LHS.get()->getType().print(StreamLHS);
  RHS.get()->getType().print(StreamRHS);
  Diags.Report(Loc,DiagType)
      << StreamLHS.str() << StreamRHS.str()
      << llvm::SMRange(LHS.get()->getMinLocation(),
                       RHS.get()->getMaxLocation());
  return ExprError();
}

static bool IsIntegerExpression(ExprResult E) {
  return E.get()->getType().getTypePtr()->isIntegerType();
}

ExprResult Sema::ActOnSubstringExpr(ASTContext &C, llvm::SMLoc Loc, ExprResult Target,
                                    ExprResult StartingPoint, ExprResult EndPoint) {
  if(StartingPoint.get() && !IsIntegerExpression(StartingPoint.get())) {
    Diags.Report(StartingPoint.get()->getLocation(),
                 diag::err_expected_integer_expr);
    return ExprError();
  }
  if(EndPoint.get() && !IsIntegerExpression(EndPoint.get())) {
    Diags.Report(EndPoint.get()->getLocation(),
                 diag::err_expected_integer_expr);
    return ExprError();
  }

  return SubstringExpr::Create(C, Loc, Target, StartingPoint, EndPoint);
}

ExprResult Sema::ActOnSubscriptExpr(ASTContext &C, llvm::SMLoc Loc, ExprResult Target,
                                    llvm::ArrayRef<ExprResult> Subscripts) {
  assert(Subscripts.size());
  auto AT = Target.get()->getType().getTypePtr()->asArrayType();
  assert(AT);
  if(AT->getDimensionCount() != Subscripts.size()) {
    Diags.Report(Subscripts[0].get()->getLocation(),
                 diag::err_array_subscript_dimension_count_mismatch) <<
                 int(AT->getDimensionCount());
    return ExprError();
  }
  //FIXME constraint
  //A subscript expression may contain array element references and function references.
  for(size_t I = 0; I < Subscripts.size(); ++I) {
    if(!IsIntegerExpression(Subscripts[I])) {
      Diags.Report(Subscripts[I].get()->getLocation(),
                   diag::err_expected_integer_expr);
      return ExprError();
    }
  }
  return ArrayElementExpr::Create(C, Loc, Target, Subscripts);
}

} // namespace flang
