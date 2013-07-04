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
  llvm::Value *VisitVarExpr(const VarExpr *E);
  llvm::Value *VisitUnaryExpr(const UnaryExpr *E);
  llvm::Value *VisitBinaryExpr(const BinaryExpr *E);
};

ScalarExprEmitter::ScalarExprEmitter(CodeGenFunction &cgf)
  : CGF(cgf), Builder(cgf.getBuilder()),
    VMContext(cgf.getLLVMContext()) {
}

llvm::Value *ScalarExprEmitter::EmitExpr(const Expr *E) {
  return Visit(E);
}

llvm::Value *ScalarExprEmitter::VisitVarExpr(const VarExpr *E) {
  auto Ptr = CGF.GetVarPtr(E->getVarDecl());
  return Builder.CreateLoad(Ptr,E->getVarDecl()->getName());
}

llvm::Value *ScalarExprEmitter::VisitUnaryExpr(const UnaryExpr *E) {
  auto Val = EmitExpr(E->getExpression());
  switch(E->getOperator()) {
  case UnaryExpr::Plus:
    return Val;
  case UnaryExpr::Minus:
    return E->getType()->isIntegerType()?
             Builder.CreateNeg(Val) :
             Builder.CreateFNeg(Val);
  }
  llvm_unreachable("invalid unary op");
  return nullptr;
}

llvm::Value *ScalarExprEmitter::VisitBinaryExpr(const BinaryExpr *E) {
  auto LHS = EmitExpr(E->getLHS());
  auto RHS = EmitExpr(E->getRHS());
  bool IsInt = E->getType()->isIntegerType();
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
  }
  return Result;
}

llvm::Value *CodeGenFunction::EmitScalarExpr(const Expr *E) {
  ScalarExprEmitter EV(*this);
  return EV.Visit(E);
}

}
} // end namespace flang
