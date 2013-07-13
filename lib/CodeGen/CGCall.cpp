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

CGFunctionInfo *CGFunctionInfo::Create(ASTContext &C,
                                       llvm::CallingConv::ID CC,
                                       llvm::FunctionType *Type,
                                       ArrayRef<ArgInfo> Arguments,
                                       ABIRetInfo RetInfo) {
  auto Info = new(C) CGFunctionInfo;
  Info->Type = Type;
  Info->CC = CC;
  Info->NumArgs = Arguments.size();
  Info->Args = new(C) ArgInfo[Info->NumArgs];
  for(unsigned I = 0; I < Info->NumArgs; ++I)
    Info->Args[I] = Arguments[I];
  Info->ReturnInfo = RetInfo;
}

RValueTy CodeGenFunction::EmitCall(const CallExpr *E) {
  CallArgList ArgList;
  return EmitCall(E->getFunction(), ArgList, E->getArguments());
}

RValueTy CodeGenFunction::EmitCall(const FunctionDecl *Function,
                                   CallArgList &ArgList,
                                   ArrayRef<Expr*> Arguments,
                                   bool ReturnsNothing) {
  CGFunction CGFunc = CGM.GetFunction(Function);
  if(Function->isExternal()) {
    // FIXME: TODO
  }
  return EmitCall(CGFunc.getFunction(), CGFunc.getInfo(),
                  ArgList, Arguments, ReturnsNothing);
}

RValueTy CodeGenFunction::EmitCall(llvm::Value *Callee,
                                   const CGFunctionInfo *FuncInfo,
                                   CallArgList &ArgList,
                                   ArrayRef<Expr*> Arguments,
                                   bool ReturnsNothing) {
  auto ArgumentInfo = FuncInfo->getArguments();
  for(size_t I = 0; I < Arguments.size(); ++I)
    EmitCallArg(ArgList.Values, Arguments[I], ArgumentInfo[I]);
  auto ReturnInfo = FuncInfo->getReturnInfo().getKind();
  if(ReturnInfo == ABIRetInfo::CharacterValueAsArg) {
    CGFunctionInfo::ArgInfo RetArgInfo;
    RetArgInfo.ABIInfo = FuncInfo->getReturnInfo().getReturnArgInfo();
    EmitCallArg(ArgList.Values, ArgList.ReturnValue.asCharacter(),
                RetArgInfo);
  }

  auto Result = Builder.CreateCall(Callee,
                                   ArgList.Values, "call");
  Result->setCallingConv(FuncInfo->getCallingConv());

  if(ReturnsNothing ||
     ReturnInfo == ABIRetInfo::Nothing)
    return RValueTy();
  else if(ReturnInfo == ABIRetInfo::ComplexValue)
    return ExtractComplexValue(Result);
  else if(ReturnInfo == ABIRetInfo::CharacterValueAsArg)
    return ArgList.ReturnValue;
  return Result;
}

RValueTy CodeGenFunction::EmitCall(CGFunction Func,
                                   ArrayRef<RValueTy> Arguments) {
  auto FuncInfo = Func.getInfo();
  SmallVector<llvm::Value*, 8> Args;
  auto ArgumentInfo = FuncInfo->getArguments();
  for(size_t I = 0; I < Arguments.size(); ++I) {
    if(Arguments[I].isScalar())
      EmitCallArg(Args, Arguments[I].asScalar(), ArgumentInfo[I]);
    else if(Arguments[I].isComplex())
      EmitCallArg(Args, Arguments[I].asComplex(), ArgumentInfo[I]);
    else
      EmitCallArg(Args, Arguments[I].asCharacter(), ArgumentInfo[I]);
  }

  auto s = Args.size();
  auto Result = Builder.CreateCall(Func.getFunction(),
                                   Args, "call");
  Result->setCallingConv(FuncInfo->getCallingConv());
  if(FuncInfo->getReturnInfo().getKind() == ABIRetInfo::ComplexValue)
    return ExtractComplexValue(Result);
  return Result;
}

void CodeGenFunction::EmitCallArg(llvm::SmallVectorImpl<llvm::Value*> &Args,
                                  const Expr *E, CGFunctionInfo::ArgInfo ArgInfo) {
  if(E->getType()->isCharacterType()) {
    EmitCallArg(Args, EmitCharacterExpr(E), ArgInfo);
    return;
  }
  switch(ArgInfo.ABIInfo.getKind()) {
  case ABIArgInfo::Value:
    Args.push_back(EmitScalarExpr(E));
    break;

  case ABIArgInfo::Reference:
    Args.push_back(EmitCallArgPtr(E));
    break;
  }
}

void CodeGenFunction::EmitCallArg(llvm::SmallVectorImpl<llvm::Value*> &Args,
                                  llvm::Value *Value, CGFunctionInfo::ArgInfo ArgInfo) {
  assert(ArgInfo.ABIInfo.getKind() == ABIArgInfo::Value);
  Args.push_back(Value);
}

void CodeGenFunction::EmitCallArg(llvm::SmallVectorImpl<llvm::Value*> &Args,
                                  ComplexValueTy Value, CGFunctionInfo::ArgInfo ArgInfo) {
  assert(ArgInfo.ABIInfo.getKind() != ABIArgInfo::Reference);
  switch(ArgInfo.ABIInfo.getKind()) {
  case ABIArgInfo::Value:
    Args.push_back(CreateComplexAggregate(Value));
    break;

  case ABIArgInfo::Expand:
    Args.push_back(Value.Re);
    Args.push_back(Value.Im);
    break;

  case ABIArgInfo::ComplexValueAsVector:
    Args.push_back(CreateComplexVector(Value));
    break;
  }
}

void CodeGenFunction::EmitCallArg(llvm::SmallVectorImpl<llvm::Value*> &Args,
                                  CharacterValueTy Value, CGFunctionInfo::ArgInfo ArgInfo) {
  assert(ArgInfo.ABIInfo.getKind() != ABIArgInfo::Reference);
  switch(ArgInfo.ABIInfo.getKind()) {
  case ABIArgInfo::Value:
    Args.push_back(CreateCharacterAggregate(Value));
    break;

  case ABIArgInfo::Expand:
    Args.push_back(Value.Ptr);
    Args.push_back(Value.Len);
    break;
  }
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


llvm::CallInst *CodeGenFunction::EmitRuntimeCall(llvm::Value *Func) {
  auto Result = Builder.CreateCall(Func);
  Result->setCallingConv(CGM.getRuntimeCC());
  return Result;
}

llvm::CallInst *CodeGenFunction::EmitRuntimeCall(llvm::Value *Func, llvm::ArrayRef<llvm::Value*> Args) {
  auto Result = Builder.CreateCall(Func, Args);
  Result->setCallingConv(CGM.getRuntimeCC());
  return Result;
}

llvm::CallInst *CodeGenFunction::EmitRuntimeCall2(llvm::Value *Func, llvm::Value *A1, llvm::Value *A2) {
  auto Result = Builder.CreateCall2(Func, A1, A2);
  Result->setCallingConv(CGM.getRuntimeCC());
  return Result;
}

}
} // end namespace flang
