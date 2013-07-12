//===--- CGCall.cpp - Encapsulate calling convention details ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// These classes wrap the information about a call or function
// definition used to handle ABI compliancy.
//
//===----------------------------------------------------------------------===//

#include "CGCall.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/Decl.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Transforms/Utils/Local.h"

namespace flang {
namespace CodeGen {

RValueTy CodeGenFunction::EmitCall(const CallExpr *E) {
  return EmitCall(E->getFunction(), E->getArguments());
}

RValueTy CodeGenFunction::EmitCall(const FunctionDecl *Function, ArrayRef<Expr*> Arguments, bool ReturnsNothing) {
  CGFunctionInfo FuncInfo;
  if(Function->isExternal()) {
    // FIXME: TODO
  } else
    FuncInfo = CGM.GetFunctionInfo(Function);
  return EmitCall(FuncInfo, Arguments, Function->getType(), ReturnsNothing);
}

RValueTy CodeGenFunction::EmitCall(CGFunctionInfo FuncInfo, ArrayRef<Expr *> Arguments,
                                   QualType ReturnType, bool ReturnsNothing) {
  SmallVector<llvm::Value*, 8> Args(Arguments.size());
  for(size_t I = 0; I < Arguments.size(); ++I)
    Args[I] = EmitCallArgPtr(Arguments[I]);
  auto Result = Builder.CreateCall(FuncInfo.getFunction(),
                                   Args, "call");
  Result->setCallingConv(FuncInfo.getCallingConv());
  if(ReturnsNothing || ReturnType.isNull())
    return RValueTy();
  if(ReturnType->isComplexType())
    return ExtractComplexValue(Result);
  return Result;
}

llvm::Value *CodeGenFunction::EmitCallArgPtr(const Expr *E) {
  if(auto Var = dyn_cast<VarExpr>(E))
    return GetVarPtr(Var->getVarDecl());
  else if(isa<ReturnedValueExpr>(E))
    return GetRetVarPtr();

  auto Value = EmitRValue(E);
  auto Temp  = CreateTempAlloca(ConvertType(E->getType()));
  EmitAssignment(Temp, Value);
  return Temp;
}

}
} // end namespace flang
