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

struct ArithmeticTypeSpec {
private:
  int Spec;
public:
  ArithmeticTypeSpec(const Type *T) {
    if(T->isIntegerType()) Spec = 1<<TST_integer;
    else if(T->isRealType()) Spec = 1<<TST_real;
    else if(T->isDoublePrecisionType()) Spec = 1<<TST_doubleprecision;
    else if(T->isComplexType()) Spec = 1<<TST_complex;
    else Spec = 0;
  }
  inline bool isArithmetic() const {
    return (Spec & ((1<<TST_integer) | (1<<TST_real) |
                   (1<<TST_doubleprecision) | (1<<TST_complex))) != 0;
  }
  inline bool is(TypeSpecifierType T) const {
    return Spec == (1<<int(T));
  }
};

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
    ArithmeticTypeSpec LHSTypeSpec(LHSType),
        RHSTypeSpec(RHSType);
    if(!LHSTypeSpec.isArithmetic()) goto typecheckInvalidOperands;
    if(!RHSTypeSpec.isArithmetic()) goto typecheckInvalidOperands;
    if(Op != BinaryExpr::Power) {
      // I2:
      // I = I1 + I2
      // R = R1 + REAL(I2)
      // D = D1 + DBLE(I2)
      // C = C1 + CMPLX(REAL(I2),0.0)
      if(RHSTypeSpec.is(TST_integer)) {
        if(LHSTypeSpec.is(TST_real))
          RHS = ConversionExpr::Create(C, RHS.get()->getLocation(),
                                       ConversionExpr::REAL,RHS);
        else if(LHSTypeSpec.is(TST_doubleprecision))
          RHS = ConversionExpr::Create(C, RHS.get()->getLocation(),
                                       ConversionExpr::DBLE,RHS);
        else if(LHSTypeSpec.is(TST_complex))
          RHS = ConversionExpr::Create(C, RHS.get()->getLocation(),
                                       ConversionExpr::CMPLX,RHS);
      }
      // R2:
      // R = REAL(I1) + R2
      // R = R1 + R2
      // D = D1 + DBLE(R2)
      // C = C1 + CMPLX(R2,0.0)
      else if(RHSTypeSpec.is(TST_real)) {
        if(LHSTypeSpec.is(TST_integer))
          LHS = ConversionExpr::Create(C, LHS.get()->getLocation(),
                                       ConversionExpr::REAL,LHS);
        else if(LHSTypeSpec.is(TST_doubleprecision))
          RHS = ConversionExpr::Create(C, RHS.get()->getLocation(),
                                       ConversionExpr::DBLE,RHS);
        else if(LHSTypeSpec.is(TST_complex))
          RHS = ConversionExpr::Create(C, RHS.get()->getLocation(),
                                       ConversionExpr::CMPLX,RHS);
      }
      // D2:
      // D = DBLE(I1) + D2
      // D = DBLE(R1) + D2
      // D = D1 + D2
      // C1 - prohibited
      else if(RHSTypeSpec.is(TST_doubleprecision)) {
        if(LHSTypeSpec.is(TST_integer) || LHSTypeSpec.is(TST_real))
          LHS = ConversionExpr::Create(C, LHS.get()->getLocation(),
                                       ConversionExpr::DBLE,LHS);
        else if(LHSTypeSpec.is(TST_complex)) goto typecheckInvalidOperands;
      }
      // C2:
      // C = CMPLX(REAL(I1),0.0) + C2
      // C = CMPLX(R1,0.0) + C2
      // D1 - prohibited
      // C = C1 + C2
      else if(RHSTypeSpec.is(TST_complex)) {
        if(LHSTypeSpec.is(TST_integer) || LHSTypeSpec.is(TST_real))
          LHS = ConversionExpr::Create(C, LHS.get()->getLocation(),
                                       ConversionExpr::CMPLX,LHS);
        else if(LHSTypeSpec.is(TST_doubleprecision)) goto typecheckInvalidOperands;
      }
    } else {
      //FIXME: TODO
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

  // realational binary expression
  case BinaryExpr::Equal: case BinaryExpr::NotEqual:
  case BinaryExpr::GreaterThan: case BinaryExpr::GreaterThanEqual:
  case BinaryExpr::LessThan: case BinaryExpr::LessThanEqual: {
    // FIXME: TODO
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
