//===----- CGCall.h - Encapsulate calling convention details ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_CGCALL_H
#define FLANG_CODEGEN_CGCALL_H

#include "ABIInfo.h"
#include "CGValue.h"
#include "flang/AST/Type.h"
#include "llvm/IR/Module.h"

namespace flang   {
namespace CodeGen {

class CallArgList {
public:
  SmallVector<llvm::Value*, 16> Values;
  RValueTy ReturnValue;

  void addReturnValueArg(RValueTy Value) {
    ReturnValue = Value;
  }
  ArrayRef<llvm::Value*> getValues() {
    return Values;
  }
};

class CGFunctionInfo {
public:
  struct ArgInfo {
    ABIArgInfo ABIInfo;

    ArgInfo() : ABIInfo(ABIArgInfo::Reference) {}
  };

  struct RetInfo {
    enum ValueKind {
      ScalarValue,
      ComplexValue
    };

    ABIRetInfo ABIInfo;
    ArgInfo    ReturnArgInfo;
    ValueKind  Kind;

    RetInfo() : ABIInfo(ABIRetInfo::Nothing), Kind(ScalarValue) {}
  };
private:
  llvm::FunctionType *Type;
  llvm::CallingConv::ID CC;
  unsigned NumArgs;
  ArgInfo *Args;
  RetInfo ReturnInfo;

  CGFunctionInfo() {}
public:
  static CGFunctionInfo *Create(ASTContext &C,
                                llvm::CallingConv::ID CC,
                                llvm::FunctionType *Type,
                                ArrayRef<ArgInfo> Arguments,
                                RetInfo Returns);

  llvm::FunctionType *getFunctionType() const {
    return Type;
  }
  llvm::CallingConv::ID getCallingConv() const {
    return CC;
  }
  ArrayRef<ArgInfo> getArguments() const {
    return ArrayRef<ArgInfo>(Args, size_t(NumArgs));
  }
  RetInfo getReturnInfo() const {
    return ReturnInfo;
  }
};

class CGFunction {
  const CGFunctionInfo *FuncInfo;
  llvm::Function *Function;
public:
  CGFunction() {}
  CGFunction(const CGFunctionInfo *Info,
             llvm::Function *Func)
    : FuncInfo(Info), Function(Func) {}

  const CGFunctionInfo *getInfo() const {
    return FuncInfo;
  }
  llvm::Function *getFunction() const {
    return Function;
  }
};

}
} // end namespace flang

#endif
