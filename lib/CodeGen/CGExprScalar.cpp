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
  auto Op = E->getOperator();
  if(Op < BinaryExpr::Plus) {
    // Complex comparison
    if(E->getLHS()->getType()->isComplexType())
      return CGF.EmitComplexRelationalExpr(Op, CGF.EmitComplexExpr(E->getLHS()),
                                           CGF.EmitComplexExpr(E->getRHS()));
  }

  auto LHS = EmitExpr(E->getLHS());
  auto RHS = EmitExpr(E->getRHS());
  bool IsInt = LHS->getType()->isIntegerTy();
  llvm::Value *Result;
  switch(Op) {
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

  default:
    return CGF.EmitScalarRelationalExpr(Op, LHS, RHS);
  }
  return Result;
}

llvm::Value *CodeGenFunction::EmitScalarRelationalExpr(BinaryExpr::Operator Op, llvm::Value *LHS,
                                                       llvm::Value *RHS) {
  auto IsInt = LHS->getType()->isIntegerTy();
  llvm::CmpInst::Predicate CmpPredicate;

  switch(Op) {
  case BinaryExpr::Eqv:
    CmpPredicate = llvm::CmpInst::ICMP_EQ;
    break;
  case BinaryExpr::Neqv:
    CmpPredicate = llvm::CmpInst::ICMP_NE;
    break;

  case BinaryExpr::Equal:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_EQ : llvm::CmpInst::FCMP_UEQ;
    break;
  case BinaryExpr::NotEqual:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_NE : llvm::CmpInst::FCMP_UNE;
    break;
  case BinaryExpr::LessThan:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_SLT : llvm::CmpInst::FCMP_ULT;
    break;
  case BinaryExpr::LessThanEqual:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_SLE : llvm::CmpInst::FCMP_ULE;
    break;
  case BinaryExpr::GreaterThan:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_SGT : llvm::CmpInst::FCMP_UGT;
    break;
  case BinaryExpr::GreaterThanEqual:
    CmpPredicate = IsInt? llvm::CmpInst::ICMP_SGE : llvm::CmpInst::FCMP_UGE;
    break;
  default:
    llvm_unreachable("unknown comparison op");
  }
  return IsInt? Builder.CreateICmp(CmpPredicate, LHS, RHS) :
                Builder.CreateFCmp(CmpPredicate, LHS, RHS);
}

llvm::Value *ScalarExprEmitter::VisitBinaryExprAnd(const BinaryExpr *E) {
  return nullptr;//FIXME
}

llvm::Value *ScalarExprEmitter::VisitBinaryExprOr(const BinaryExpr *E) {
  return nullptr;//FIXME
}

llvm::Value *CodeGenFunction::EmitScalarToScalarConversion(llvm::Value *Value,
                                                           QualType Target) {
  auto ValueType = Value->getType();

  if(ValueType->isIntegerTy()) {
    if(Target->isIntegerType()) {
      return Value; // FIXME: Kinds
    } else {
      assert(Target->isRealType());
      return Builder.CreateSIToFP(Value, ConvertType(Target));
    }
  } else {
    assert(ValueType->isFloatingPointTy());
    if(Target->isRealType()) {
      return Value; // FIXME: Kinds
    } else {
      assert(Target->isIntegerType());
      return Builder.CreateFPToSI(Value, ConvertType(Target));
    }
  }
  return Value;
}

llvm::Value *ScalarExprEmitter::VisitImplicitCastExpr(const ImplicitCastExpr *E) {
  auto Input = E->getExpression();
  if(Input->getType()->isComplexType())
    return CGF.EmitComplexToScalarConversion(CGF.EmitComplexExpr(Input), E->getType());
  return CGF.EmitScalarToScalarConversion(EmitExpr(Input), E->getType());
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

llvm::Value *CodeGenFunction::GetConstantOne(QualType T) {
  auto Type = ConvertType(T);
  return Type->isIntegerTy()? llvm::ConstantInt::get(Type, 1) :
                              llvm::ConstantFP::get(Type, 1.0);
}

llvm::Value *CodeGenFunction::GetConstantZero(QualType T) {
  auto Type = ConvertType(T);
  return Type->isIntegerTy()? llvm::ConstantInt::get(Type, 0) :
                              llvm::ConstantFP::get(Type, 0.0);
}

}
} // end namespace flang
