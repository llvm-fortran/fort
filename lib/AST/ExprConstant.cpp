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
#include <limits>

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
  bool VisitArrayConstructorExpr(const ArrayConstructorExpr *E);
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

bool ConstExprVerifier::VisitArrayConstructorExpr(const ArrayConstructorExpr *E) {
  for(auto I : E->getItems()) {
    if(!Eval(I))
      return false;
  }
  return true;
}

struct IntValueTy : public llvm::APInt {

  IntValueTy() {}
  IntValueTy(uint64_t I) :
    llvm::APInt(64, I) {}

  template<typename T = int64_t>
  bool IsProperSignedInt() const {
    auto u64 = getLimitedValue();
    if(isNonNegative())
      return u64 <= uint64_t(std::numeric_limits<T>::max());
    else
      return T(int64_t(u64)) >= (std::numeric_limits<T>::min());
  }

  void operator=(const llvm::APInt &I) {
    llvm::APInt::operator =(I);
  }

  template<typename T = int64_t>
  bool Assign(const llvm::APInt &I) {
    auto u64 = I.getLimitedValue();
    if(u64 <= uint64_t(std::numeric_limits<T>::max())) {
      *this = IntValueTy(u64);
      return true;
    }
    *this = IntValueTy(1);
    return false;
  }
};

/// Evaluates 64 bit signed integers.
class IntExprEvaluator: public ConstExprVisitor<IntExprEvaluator,
                                                bool> {
  IntValueTy Result;
public:
  IntExprEvaluator() {}

  bool CheckResult(bool Overflow);

  bool Eval(const Expr *E);
  bool VisitExpr(const Expr *E);
  bool VisitIntegerConstantExpr(const IntegerConstantExpr *E);
  bool VisitUnaryExprMinus(const UnaryExpr *E);
  bool VisitUnaryExprPlus(const UnaryExpr *E);
  bool VisitBinaryExpr(const BinaryExpr *E);
  bool VisitVarExpr(const VarExpr *E);

  int64_t getResult() const;
};

int64_t IntExprEvaluator::getResult() const {
  if(Result.IsProperSignedInt())
    return int64_t(Result.getLimitedValue());
  return 1;
}

bool IntExprEvaluator::Eval(const Expr *E) {
  if(E->getType()->isIntegerType())
    return Visit(E);
  return false;
}

bool IntExprEvaluator::CheckResult(bool Overflow) {
  if(Overflow || !Result.IsProperSignedInt())
    return false;
  return true;
}

bool IntExprEvaluator::VisitExpr(const Expr *E) {
  return false;
}

bool IntExprEvaluator::VisitIntegerConstantExpr(const IntegerConstantExpr *E) {
  return Result.Assign(E->getValue());
}

bool IntExprEvaluator::VisitUnaryExprMinus(const UnaryExpr *E) {
  if(!Eval(E->getExpression())) return false;
  bool Overflow = false;
  Result = IntValueTy(0).ssub_ov(Result, Overflow);
  return CheckResult(Overflow);
}

bool IntExprEvaluator::VisitUnaryExprPlus(const UnaryExpr *E) {
  return Eval(E->getExpression());
}

bool IntExprEvaluator::VisitBinaryExpr(const BinaryExpr *E) {
  if(!Eval(E->getRHS())) return false;
  IntValueTy RHS(Result);
  if(!Eval(E->getLHS())) return false;

  bool Overflow = false;
  switch(E->getOperator()) {
  case BinaryExpr::Plus:
    Result = Result.sadd_ov(RHS, Overflow);
    break;
  case BinaryExpr::Minus:
    Result = Result.ssub_ov(RHS, Overflow);
    break;
  case BinaryExpr::Multiply:
    Result = Result.smul_ov(RHS, Overflow);
    break;
  case BinaryExpr::Divide:
    Result = Result.sdiv_ov(RHS, Overflow);
    break;
  case BinaryExpr::Power: {
    if(RHS.isNegative()) return false;
    uint64_t N = RHS.getLimitedValue();
    IntValueTy Sum(1);
    for(uint64_t I = 0; I < N; ++I) {
      Overflow = false;
      Sum = Sum.smul_ov(Result, Overflow);
      if(Overflow || !Sum.IsProperSignedInt())
        return false;
    }
    Result = Sum;
    break;
  }
  default:
    return false;
  }
  return CheckResult(Overflow);
}

bool IntExprEvaluator::VisitVarExpr(const VarExpr *E) {
  auto VD = E->getVarDecl();
  if(VD->isParameter())
    return Eval(VD->getInit());
  return false;
}

bool Expr::EvaluateAsInt(int64_t &Result, const ASTContext &Ctx) const {
  IntExprEvaluator EV;
  auto Success = EV.Eval(this);
  Result = EV.getResult();
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

uint64_t EvaluatedArraySpec::EvaluateOffset(int64_t Index) const {
  auto I = Index - LowerBound;
  assert(I >= 0);
  return uint64_t(I);
}

bool ArraySpec::Evaluate(EvaluatedArraySpec &Spec, const ASTContext &Ctx) const {
  return false;
}

bool ExplicitShapeSpec::Evaluate(EvaluatedArraySpec &Spec, const ASTContext &Ctx) const {
  if(getLowerBound()) {
    if(!getLowerBound()->EvaluateAsInt(Spec.LowerBound, Ctx))
      return false;
  } else Spec.LowerBound = 1;
  if(!getUpperBound()->EvaluateAsInt(Spec.UpperBound, Ctx))
    return false;
  auto Sz = Spec.UpperBound - Spec.LowerBound + 1;
  assert(Sz > 0);
  Spec.Size = uint64_t(Sz);
  return true;
}

static
bool EvaluateDimensions(const ArrayType *T,
                        llvm::MutableArrayRef<EvaluatedArraySpec> Dims,
                        const ASTContext &Ctx) {
  assert(T->getDimensionCount() == Dims.size());
  auto Dimensions = T->getDimensions();
  for(size_t I = 0; I < Dimensions.size(); ++I) {
    if(!Dimensions[I]->Evaluate(Dims[I], Ctx))
      return false;
  }
  return true;
}

bool ArrayElementExpr::EvaluateOffset(ASTContext &Ctx, uint64_t &Offset) const {
  auto ATy = getTarget()->getType()->asArrayType();
  SmallVector<EvaluatedArraySpec, 8> Dims(ATy->getDimensionCount());
  if(!EvaluateDimensions(ATy, Dims, Ctx))
    return false;
  auto Subscripts = getSubscripts();
  Offset = 0;
  uint64_t DimSizes = 0;
  for(size_t I = 0; I < Dims.size(); ++I) {
    int64_t Index;
    if(!Subscripts[I]->EvaluateAsInt(Index, Ctx))
      return false;
    if(I == 0) {
      Offset = Dims[I].EvaluateOffset(Index);
      DimSizes = Dims[I].Size;
    } else {
      Offset += DimSizes * Dims[I].EvaluateOffset(Index);
      DimSizes *= Dims[I].Size;
    }
  }
  return true;
}


} // end namespace flang
