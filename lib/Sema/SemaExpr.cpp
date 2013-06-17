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

// FIXME: verify return type for binary expressions.
ExprResult Sema::ActOnBinaryExpr(ASTContext &C, llvm::SMLoc Loc,
                                 BinaryExpr::Operator Op,
                                 ExprResult LHS,ExprResult RHS) {
  const Type *LHSType = LHS.get()->getType().getTypePtr();
  const Type *RHSType = RHS.get()->getType().getTypePtr();

  switch(Op) {
  // Arithmetic binary expression
  case BinaryExpr::Plus: case BinaryExpr::Minus:
  case BinaryExpr::Multiply: case BinaryExpr::Divide:
  case BinaryExpr::Power: {
    TypeSpecifierType LHSTypeSpec = GetArithmeticTypeSpec(LHSType);
    TypeSpecifierType RHSTypeSpec = GetArithmeticTypeSpec(RHSType);

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
        RHS = ConversionExpr::Create(C, RHS.get()->getLocation(),
                                     ConversionExpr::REAL,RHS);
        break;
      case TST_doubleprecision:
        RHS = ConversionExpr::Create(C, RHS.get()->getLocation(),
                                     ConversionExpr::DBLE,RHS);
        break;
      case TST_complex:
        RHS = ConversionExpr::Create(C, RHS.get()->getLocation(),
                                     ConversionExpr::CMPLX,RHS);
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
        LHS = ConversionExpr::Create(C, LHS.get()->getLocation(),
                                     ConversionExpr::REAL,LHS);
        break;
      case TST_real: break;
      case TST_doubleprecision:
        RHS = ConversionExpr::Create(C, RHS.get()->getLocation(),
                                     ConversionExpr::DBLE,RHS);
        break;
      case TST_complex:
        RHS = ConversionExpr::Create(C, RHS.get()->getLocation(),
                                     ConversionExpr::CMPLX,RHS);
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
        LHS = ConversionExpr::Create(C, LHS.get()->getLocation(),
                                     ConversionExpr::DBLE,LHS);
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
        LHS = ConversionExpr::Create(C, LHS.get()->getLocation(),
                                     ConversionExpr::CMPLX,LHS);
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
    if(!LHSType->isLogicalType()) goto typecheckInvalidOperands;
    if(!RHSType->isLogicalType()) goto typecheckInvalidOperands;
    break;
  }

  // Character binary expression
  case BinaryExpr::Concat: {
    if(!LHSType->isCharacterType()) goto typecheckInvalidOperands;
    if(!RHSType->isCharacterType()) goto typecheckInvalidOperands;
    break;
  }

  // relational binary expression
  // FIXME: TEST
  case BinaryExpr::Equal: case BinaryExpr::NotEqual:
  case BinaryExpr::GreaterThan: case BinaryExpr::GreaterThanEqual:
  case BinaryExpr::LessThan: case BinaryExpr::LessThanEqual: {
    // Character relational expression
    if(LHSType->isCharacterType() && RHSType->isCharacterType()) break;

    // Arithmetic relational expression
    TypeSpecifierType LHSTypeSpec = GetArithmeticTypeSpec(LHSType);
    TypeSpecifierType RHSTypeSpec = GetArithmeticTypeSpec(RHSType);

    if(LHSTypeSpec == TST_unspecified || RHSTypeSpec == TST_unspecified)
      goto typecheckInvalidOperands;

    // A complex operand is permitted only when the relational operator is .EQ. or .NE.
    if((LHSTypeSpec == TST_complex ||
        RHSTypeSpec == TST_complex) &&
       Op != BinaryExpr::Equal && Op != BinaryExpr::NotEqual) {
      //FIXME: TODO error
    }

    // typeof(e1) != typeof(e1) =>
    //   e1 <relop> e2 = ((e1) - (e2)) <relop> 0
    if(LHSTypeSpec != RHSTypeSpec) {
      LHS = ActOnBinaryExpr(C, Loc, BinaryExpr::Minus, LHS, RHS);
      RHS = IntegerConstantExpr::Create(C, Loc, "0");
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
  Diags.Report(Loc,diag::err_typecheck_invalid_operands)
      << StreamLHS.str() << StreamRHS.str();
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
  const ArrayType *AT = Target.get()->getType().getTypePtr()->asArrayType();
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
