//===--- CGExprScalar.cpp - Emit LLVM Code for Scalar Exprs ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Expr nodes with scalar LLVM types as LLVM code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/ExprVisitor.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CFG.h"

namespace flang {
namespace CodeGen {

class ScalarExprEmitter
  : public ConstExprVisitor<ScalarExprEmitter, llvm::Value*> {
  CodeGenFunction &CGF;
  CGBuilderTy &Builder;
  llvm::LLVMContext &VMContext;
public:

  ScalarExprEmitter(CodeGenFunction &cgf);

  llvm::Value *EmitExpr(const Expr *E);
  llvm::Value *VisitIntegerConstantExpr(const IntegerConstantExpr *E);
  llvm::Value *VisitRealConstantExpr(const RealConstantExpr *E);
  llvm::Value *VisitLogicalConstantExpr(const LogicalConstantExpr *E);
  llvm::Value *VisitVarExpr(const VarExpr *E);
  llvm::Value *VisitReturnedValueExpr(const ReturnedValueExpr *E);
  llvm::Value *VisitUnaryExprPlus(const UnaryExpr *E);
  llvm::Value *VisitUnaryExprMinus(const UnaryExpr *E);
  llvm::Value *VisitUnaryExprNot(const UnaryExpr *E);
  llvm::Value *VisitBinaryExpr(const BinaryExpr *E);
  llvm::Value *VisitBinaryExprAnd(const BinaryExpr *E);
  llvm::Value *VisitBinaryExprOr(const BinaryExpr *E);
  llvm::Value *VisitImplicitCastExpr(const ImplicitCastExpr *E);
  llvm::Value *VisitCallExpr(const CallExpr *E);
  llvm::Value *VisitIntrinsicCallExpr(const IntrinsicCallExpr *E);

};

ScalarExprEmitter::ScalarExprEmitter(CodeGenFunction &cgf)
  : CGF(cgf), Builder(cgf.getBuilder()),
    VMContext(cgf.getLLVMContext()) {
}

llvm::Value *ScalarExprEmitter::EmitExpr(const Expr *E) {
  return Visit(E);
}

llvm::Value *CodeGenFunction::EmitIntegerConstantExpr(const IntegerConstantExpr *E) {
  return llvm::ConstantInt::get(Builder.getInt32Ty(), E->getValue().sextOrTrunc(32));
}

llvm::Value *ScalarExprEmitter::VisitIntegerConstantExpr(const IntegerConstantExpr *E) {
  return CGF.EmitIntegerConstantExpr(E);
}

llvm::Value *ScalarExprEmitter::VisitRealConstantExpr(const RealConstantExpr *E) {
  return llvm::ConstantFP::get(VMContext, E->getValue());
}

llvm::Value *ScalarExprEmitter::VisitLogicalConstantExpr(const LogicalConstantExpr *E) {
  return Builder.getInt1(E->isTrue());
}

llvm::Value *ScalarExprEmitter::VisitVarExpr(const VarExpr *E) {
  auto Ptr = CGF.GetVarPtr(E->getVarDecl());
  return Builder.CreateLoad(Ptr,E->getVarDecl()->getName());
}

llvm::Value *ScalarExprEmitter::VisitReturnedValueExpr(const ReturnedValueExpr *E) {
  return nullptr; // FIXME
}

llvm::Value *ScalarExprEmitter::VisitUnaryExprPlus(const UnaryExpr *E) {
  return EmitExpr(E->getExpression());
}

llvm::Value *ScalarExprEmitter::VisitUnaryExprMinus(const UnaryExpr *E) {
  auto Val = EmitExpr(E->getExpression());
  return E->getType()->isIntegerType()?
           Builder.CreateNeg(Val) :
           Builder.CreateFNeg(Val);
}

llvm::Value *ScalarExprEmitter::VisitUnaryExprNot(const UnaryExpr *E) {
  auto Val = EmitExpr(E->getExpression());
  return Builder.CreateXor(Val,1);
}

llvm::Value *ScalarExprEmitter::VisitBinaryExpr(const BinaryExpr *E) {
  auto LHS = EmitExpr(E->getLHS());
  auto RHS = EmitExpr(E->getRHS());
  bool IsInt = LHS->getType()->isIntegerTy();
  llvm::CmpInst::Predicate CmpPredicate;
  llvm::Value *Result;
  switch(E->getOperator()) {
  case BinaryExpr::Plus:
    Result = IsInt?  Builder.CreateAdd(LHS, RHS) :
                     Builder.CreateFAdd(LHS, RHS);
    break;
  case BinaryExpr::Minus:
    Result = IsInt?  Builder.CreateSub(LHS, RHS) :
                     Builder.CreateFSub(LHS, RHS);
    break;
  case BinaryExpr::Multiply:
    Result = IsInt?  Builder.CreateMul(LHS, RHS) :
                     Builder.CreateFMul(LHS, RHS);
    break;
  case BinaryExpr::Divide:
    Result = IsInt?  Builder.CreateSDiv(LHS, RHS) :
                     Builder.CreateFDiv(LHS, RHS);
    break;
  case BinaryExpr::Power:
    break;

  case BinaryExpr::Eqv:
    CmpPredicate = llvm::CmpInst::ICMP_EQ;
    goto CreateCmp;
  case BinaryExpr::Neqv:
    CmpPredicate = llvm::CmpInst::ICMP_NE;
    goto CreateCmp;

  case BinaryExpr::Equal:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_EQ : llvm::CmpInst::FCMP_UEQ;
    goto CreateCmp;
  case BinaryExpr::NotEqual:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_NE : llvm::CmpInst::FCMP_UNE;
    goto CreateCmp;
  case BinaryExpr::LessThan:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_SLT : llvm::CmpInst::FCMP_ULT;
    goto CreateCmp;
  case BinaryExpr::LessThanEqual:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_SLE : llvm::CmpInst::FCMP_ULE;
    goto CreateCmp;
  case BinaryExpr::GreaterThan:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_SGT : llvm::CmpInst::FCMP_UGT;
    goto CreateCmp;
  case BinaryExpr::GreaterThanEqual:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_SGE : llvm::CmpInst::FCMP_UGE;
    goto CreateCmp;
  }
  return Result;
CreateCmp:
  return IsInt? Builder.CreateICmp(CmpPredicate, LHS, RHS) :
                Builder.CreateFCmp(CmpPredicate, LHS, RHS);
}

llvm::Value *ScalarExprEmitter::VisitBinaryExprAnd(const BinaryExpr *E) {
  return nullptr;//FIXME
}

llvm::Value *ScalarExprEmitter::VisitBinaryExprOr(const BinaryExpr *E) {
  return nullptr;//FIXME
}

llvm::Value *ScalarExprEmitter::VisitImplicitCastExpr(const ImplicitCastExpr *E) {
  auto Val = EmitExpr(E->getExpression());
  return Val;//FIXME
}

llvm::Value *ScalarExprEmitter::VisitCallExpr(const CallExpr *E) {
  return nullptr;//FIXME
}

llvm::Value *ScalarExprEmitter::VisitIntrinsicCallExpr(const IntrinsicCallExpr *E) {
  return nullptr;//FIXME
}

llvm::Value *CodeGenFunction::EmitScalarExpr(const Expr *E) {
  ScalarExprEmitter EV(*this);
  return EV.Visit(E);
}

llvm::Value *CodeGenFunction::EmitLogicalScalarExpr(const Expr *E) {
  return EmitScalarExpr(E);
}

}
} // end namespace flang
