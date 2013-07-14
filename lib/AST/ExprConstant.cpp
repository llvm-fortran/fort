//===--- ExprConstant.cpp - Expression Constant Evaluator -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Expr constant evaluator.
//
//===----------------------------------------------------------------------===//

#include "flang/AST/Decl.h"
#include "flang/AST/Expr.h"
#include "flang/AST/ExprVisitor.h"

namespace flang {

class ConstExprVerifier: public ConstExprVisitor<ConstExprVerifier,
                                                 bool> {
  SmallVectorImpl<const Expr *> *NonConstants;
public:
  ConstExprVerifier(SmallVectorImpl<const Expr *> *NonConst = nullptr)
    : NonConstants(NonConst) {}

  bool Eval(const Expr *E);
  bool VisitExpr(const Expr *E);
  bool VisitUnaryExpr(const UnaryExpr *E);
  bool VisitBinaryExpr(const BinaryExpr *E);
  bool VisitImplicitCastExpr(const ImplicitCastExpr *E);
  bool VisitVarExpr(const VarExpr *E);
};

bool ConstExprVerifier::Eval(const Expr *E) {
  if(isa<ConstantExpr>(E))
    return true;
  return Visit(E);
}

bool ConstExprVerifier::VisitExpr(const Expr *E) {
  if(NonConstants)
    NonConstants->push_back(E);
  return false;
}

bool ConstExprVerifier::VisitUnaryExpr(const UnaryExpr *E) {
  return Eval(E->getExpression());
}

bool ConstExprVerifier::VisitBinaryExpr(const BinaryExpr *E) {
  auto LHS = Eval(E->getLHS());
  auto RHS = Eval(E->getRHS());
  return LHS && RHS;
}

bool ConstExprVerifier::VisitImplicitCastExpr(const ImplicitCastExpr *E) {
  return Eval(E->getExpression());
}

bool ConstExprVerifier::VisitVarExpr(const VarExpr *E) {
  if(E->getVarDecl()->isParameter())
    return Eval(E->getVarDecl()->getInit());
  if(NonConstants)
    NonConstants->push_back(E);
  return false;
}

class IntExprEvaluator: public ConstExprVisitor<IntExprEvaluator,
                                                bool> {
  llvm::APSInt &Result;
public:
  IntExprEvaluator(llvm::APSInt &I)
    : Result(I) {}

  bool Eval(const Expr *E);
  bool VisitExpr(const Expr *E);
  bool VisitIntegerConstantExpr(const IntegerConstantExpr *E);
  bool VisitUnaryExprMinus(const UnaryExpr *E);
  bool VisitUnaryExprPlus(const UnaryExpr *E);
  bool VisitBinaryExpr(const BinaryExpr *E);
  bool VisitVarExpr(const VarExpr *E);
};

bool IntExprEvaluator::Eval(const Expr *E) {
  if(E->getType()->isIntegerType())
    return Visit(E);
  return false;
}

bool IntExprEvaluator::VisitExpr(const Expr *E) {
  return false;
}

bool IntExprEvaluator::VisitIntegerConstantExpr(const IntegerConstantExpr *E) {
  Result = E->getValue();
  return true;
}

bool IntExprEvaluator::VisitUnaryExprMinus(const UnaryExpr *E) {
  if(!Eval(E->getExpression())) return false;
  Result.setIsSigned(!Result.isSigned());
}

bool IntExprEvaluator::VisitUnaryExprPlus(const UnaryExpr *E) {
  return Eval(E->getExpression());
}

bool IntExprEvaluator::VisitBinaryExpr(const BinaryExpr *E) {
  if(!Eval(E->getRHS())) return false;
  llvm::APSInt RHS(Result);
  if(!Eval(E->getLHS())) return false;

  switch(E->getOperator()) {
  case BinaryExpr::Plus:
    Result += RHS;
    break;
  case BinaryExpr::Minus:
    Result -= RHS;
    break;
  case BinaryExpr::Multiply:
    Result *= RHS;
    break;
  case BinaryExpr::Divide:
    Result /= RHS;
    break;
  case BinaryExpr::Power: {
    if(llvm::APSInt::isSameValue(RHS, llvm::APSInt(0))) {
      Result = RHS;
      break;
    } else if(RHS.isUnsigned()) {
      llvm::APSInt R(Result);
      for(llvm::APSInt I(1); I < RHS; ++I) {
        Result *= R;
      }
    } else return false;
    break;
  }
  default:
    return false;
  }
  return true;
}

bool IntExprEvaluator::VisitVarExpr(const VarExpr *E) {
  auto VD = E->getVarDecl();
  if(VD->isParameter())
    return Eval(VD->getInit());
  return false;
}

bool Expr::EvaluateAsInt(llvm::APSInt &Result, const ASTContext &Ctx) const {
  IntExprEvaluator EV(Result);
  auto Success = EV.Eval(this);
  if(Success) {
    if(llvm::APSInt::isSameValue(Result, llvm::APSInt(0)))
      Result.setIsUnsigned(true);
  }
  return Success;
}

bool Expr::isEvaluatable(const ASTContext &Ctx) const {
  ConstExprVerifier EV;
  return EV.Eval(this);
}

void Expr::GatherNonEvaluatableExpressions(const ASTContext &Ctx,
                                           SmallVectorImpl<const Expr*> &Result) {
  ConstExprVerifier EV(&Result);
  EV.Eval(this);
  if(Result.size() == 0)
    Result.push_back(this);
}

} // end namespace flang
