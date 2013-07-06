//===--- CGIntrinsic.cpp - Emit LLVM Code for Intrinsic calls ------------===//
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

RValueTy CodeGenFunction::EmitIntrinsicCall(const IntrinsicCallExpr *E) {
  auto Func = intrinsic::getGenericFunctionKind(E->getIntrinsicFunction());
  auto Group = intrinsic::getFunctionGroup(Func);
  auto Args = E->getArguments();

  switch(Group) {
  case intrinsic::GROUP_CONVERSION:
    break;

  case intrinsic::GROUP_TRUNCATION:
    return EmitIntrinsicCallScalarTruncation(Func, EmitScalarExpr(Args[0]),
                                             E->getType());

  case intrinsic::GROUP_COMPLEX:
    return EmitIntrinsicCallComplex(Func, EmitComplexExpr(Args[0]));

  case intrinsic::GROUP_MATHS:
    if(Args[0]->getType()->isComplexType())
      return EmitIntrinsicCallComplexMath(Func, EmitComplexExpr(Args[0]));
    return EmitIntrinsicCallScalarMath(Func, EmitScalarExpr(Args[0]),
                                       Args.size() == 2?
                                        EmitScalarExpr(Args[1]) : nullptr);
  default:
    break;
  }

  // other intrinsics
  switch(Func) {
  case intrinsic::DPROD: {
    auto TargetType = getContext().DoublePrecisionTy;
    auto A1 = EmitScalarExpr(Args[0]);
    auto A2 = EmitScalarExpr(Args[1]);
    return Builder.CreateFMul(EmitScalarToScalarConversion(A1, TargetType),
                              EmitScalarToScalarConversion(A2, TargetType));
  }

  }

  return RValueTy();
}

llvm::Value *CodeGenFunction::EmitIntrinsicCallScalarTruncation(intrinsic::FunctionKind Func,
                                                                llvm::Value *Value,
                                                                QualType ResultType) {
  llvm::Value *FuncDecl = nullptr;
  auto ValueType = Value->getType();
  switch(Func) {
  case intrinsic::AINT:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::trunc, ValueType);
    break;
  case intrinsic::ANINT:
  case intrinsic::NINT:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::rint, ValueType);
    break;
  }

  auto Result = Builder.CreateCall(FuncDecl, Value);
  if(Func == intrinsic::NINT)
    return EmitScalarToScalarConversion(Result, ResultType);
  return Result;
}

llvm::Value* CodeGenFunction::EmitIntrinsicCallScalarMath(intrinsic::FunctionKind Func,
                                                          llvm::Value *A1, llvm::Value *A2) {
  llvm::Value *FuncDecl = nullptr;
  auto ValueType = A1->getType();
  switch(Func) {
  case intrinsic::ABS:
    if(ValueType->isIntegerTy()) {
      auto Condition = Builder.CreateICmpSGE(A1, llvm::ConstantInt::get(ValueType, 0));
      return Builder.CreateSelect(Condition, A1, Builder.CreateNeg(A1));
    }
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::fabs, ValueType);
    break;
  case intrinsic::MOD:
    if(ValueType->isIntegerTy())
      return Builder.CreateSRem(A1, A2);
    else
      return Builder.CreateFRem(A1, A2);
    break;
  case intrinsic::SQRT:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::sqrt, ValueType);
    break;
  case intrinsic::EXP:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::exp, ValueType);
    break;
  case intrinsic::LOG:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::log, ValueType);
    break;
  case intrinsic::LOG10:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::log10, ValueType);
    break;
  case intrinsic::SIN:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::sin, ValueType);
    break;
  case intrinsic::COS:
    FuncDecl = GetIntrinsicFunction(llvm::Intrinsic::cos, ValueType);
    break;
  case intrinsic::TAN:
    break;
  case intrinsic::ASIN:
    break;
  case intrinsic::ACOS:
    break;
  case intrinsic::ATAN:
    break;
  case intrinsic::ATAN2:
    break;
  case intrinsic::SINH:
    break;
  case intrinsic::COSH:
    break;
  case intrinsic::TANH:
    break;
  default:
    llvm_unreachable("invalid scalar math intrinsic");
  }
  if(A2)
    return Builder.CreateCall2(FuncDecl, A1, A2);
  return Builder.CreateCall(FuncDecl, A1);
}

ComplexValueTy CodeGenFunction::EmitIntrinsicCallComplexMath(intrinsic::FunctionKind Func,
                                                             ComplexValueTy Value) {
  // FIXME:
  switch(Func) {
  case intrinsic::ABS:
    break;
  case intrinsic::SQRT:
    break;
  case intrinsic::EXP:
    break;
  case intrinsic::LOG:
    break;
  case intrinsic::SIN:
    break;
  case intrinsic::COS:
    break;
  default:
    llvm_unreachable("invalid complex math intrinsic");
  }
  return ComplexValueTy();
}

}
} // end namespace flang
