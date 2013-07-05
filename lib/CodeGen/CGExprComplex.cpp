//===--- CGExprComplex.cpp - Emit LLVM Code for Complex Exprs -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Expr nodes with complex types as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/ExprVisitor.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/ADT/APFloat.h"

namespace flang {
namespace CodeGen {

class ComplexExprEmitter
  : public ConstExprVisitor<ComplexExprEmitter, ComplexValueTy> {
  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
  llvm::LLVMContext &VMContext;
public:

  ComplexExprEmitter(CodeGenFunction &cgf);

  ComplexValueTy EmitExpr(const Expr *E);
  ComplexValueTy VisitComplexConstantExpr(const ComplexConstantExpr *E);
  ComplexValueTy VisitVarExpr(const VarExpr *E);
  ComplexValueTy VisitReturnedValueExpr(const ReturnedValueExpr *E);
  ComplexValueTy VisitUnaryExprPlus(const UnaryExpr *E);
  ComplexValueTy VisitUnaryExprMinus(const UnaryExpr *E);
  ComplexValueTy VisitBinaryExpr(const BinaryExpr *E);
  ComplexValueTy VisitImplicitCastExpr(const ImplicitCastExpr *E);
  ComplexValueTy VisitCallExpr(const CallExpr *E);
  ComplexValueTy VisitIntrinsicCallExpr(const IntrinsicCallExpr *E);

};

ComplexExprEmitter::ComplexExprEmitter(CodeGenFunction &cgf)
  : CGF(cgf), Builder(cgf.getBuilder()),
    VMContext(cgf.getLLVMContext()) {
}

ComplexValueTy ComplexExprEmitter::EmitExpr(const Expr *E) {
  return Visit(E);
}

ComplexValueTy ComplexExprEmitter::VisitComplexConstantExpr(const ComplexConstantExpr *E) {
  return ComplexValueTy(llvm::ConstantFP::get(VMContext, E->getRealValue()),
                        llvm::ConstantFP::get(VMContext, E->getImaginaryValue()));
}

ComplexValueTy CodeGenFunction::EmitComplexLoad(llvm::Value *Ptr, bool IsVolatile) {
  auto Re = Builder.CreateLoad(Builder.CreateStructGEP(Ptr,0), IsVolatile);
  auto Im = Builder.CreateLoad(Builder.CreateStructGEP(Ptr,1), IsVolatile);
  return ComplexValueTy(Re, Im);
}

void CodeGenFunction::EmitComplexStore(ComplexValueTy Value, llvm::Value *Ptr,
                                       bool IsVolatile) {
  Builder.CreateStore(Value.Re, Builder.CreateStructGEP(Ptr,0), IsVolatile);
  Builder.CreateStore(Value.Im, Builder.CreateStructGEP(Ptr,1), IsVolatile);
}

ComplexValueTy ComplexExprEmitter::VisitVarExpr(const VarExpr *E) {
  auto Ptr = CGF.GetVarPtr(E->getVarDecl());
  return CGF.EmitComplexLoad(Ptr);
}

ComplexValueTy ComplexExprEmitter::VisitReturnedValueExpr(const ReturnedValueExpr *E) {
  return ComplexValueTy(); // FIXME
}

ComplexValueTy ComplexExprEmitter::VisitUnaryExprPlus(const UnaryExpr *E) {
  return EmitExpr(E->getExpression());
}

ComplexValueTy ComplexExprEmitter::VisitUnaryExprMinus(const UnaryExpr *E) {
  auto Val = EmitExpr(E->getExpression());
  return ComplexValueTy(Builder.CreateFNeg(Val.Re),
                        Builder.CreateFNeg(Val.Im));
}

ComplexValueTy ComplexExprEmitter::VisitBinaryExpr(const BinaryExpr *E) {
  auto LHS = EmitExpr(E->getLHS());
  auto RHS = EmitExpr(E->getRHS());
  ComplexValueTy Result;

  switch(E->getOperator()) {
  case BinaryExpr::Plus:
    Result.Re = Builder.CreateFAdd(LHS.Re, RHS.Re);
    Result.Im = Builder.CreateFAdd(LHS.Im, RHS.Im);
    break;

  case BinaryExpr::Minus:
    Result.Re = Builder.CreateFSub(LHS.Re, RHS.Re);
    Result.Im = Builder.CreateFSub(LHS.Im, RHS.Im);
    break;

  case BinaryExpr::Multiply: {
    // (a+ib) * (c+id) = (ac - bd) + i(bc + ad)
    auto Left = Builder.CreateFMul(LHS.Re, RHS.Re);
    auto Right = Builder.CreateFMul(LHS.Im, RHS.Im);
    Result.Re = Builder.CreateFSub(Left, Right);

    Left = Builder.CreateFMul(LHS.Im, RHS.Re);
    Right = Builder.CreateFMul(LHS.Re, RHS.Im);
    Result.Im = Builder.CreateFAdd(Left, Right);
    break;
  }

  case BinaryExpr::Divide: {
    // (a+ib) / (c+id) = ((ac+bd)/(cc+dd)) + i((bc-ad)/(cc+dd))
    auto Tmp1 = Builder.CreateFMul(LHS.Re, RHS.Re); // a*c
    auto Tmp2 = Builder.CreateFMul(LHS.Im, RHS.Im); // b*d
    auto Tmp3 = Builder.CreateFAdd(Tmp1, Tmp2); // ac+bd

    auto Tmp4 = Builder.CreateFMul(RHS.Re, RHS.Re); // c*c
    auto Tmp5 = Builder.CreateFMul(RHS.Im, RHS.Im); // d*d
    auto Tmp6 = Builder.CreateFAdd(Tmp4, Tmp5); // cc+dd

    auto Tmp7 = Builder.CreateFMul(LHS.Im, RHS.Re); // b*c
    auto Tmp8 = Builder.CreateFMul(LHS.Re, RHS.Im); // a*d
    auto Tmp9 = Builder.CreateFSub(Tmp7, Tmp8); // bc-ad

    Result.Re = Builder.CreateFDiv(Tmp3, Tmp6);
    Result.Im = Builder.CreateFDiv(Tmp9, Tmp6);
    break;
  }

  case BinaryExpr::Power:
    // FIXME: TODO
    break;
  }
  return Result;
}

ComplexValueTy ComplexExprEmitter::VisitImplicitCastExpr(const ImplicitCastExpr *E) {
  return ComplexValueTy(); // FIXME
}

ComplexValueTy ComplexExprEmitter::VisitCallExpr(const CallExpr *E) {
  return ComplexValueTy(); // FIXME
}

ComplexValueTy ComplexExprEmitter::VisitIntrinsicCallExpr(const IntrinsicCallExpr *E) {
  return ComplexValueTy(); // FIXME
}

ComplexValueTy CodeGenFunction::EmitComplexExpr(const Expr *E) {
  ComplexExprEmitter EV(*this);
  return EV.EmitExpr(E);
}

}
} // end namespace flang
