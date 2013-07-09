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

#include "CGValue.h"
#include "flang/AST/Type.h"
#include "llvm/IR/Module.h"

namespace flang   {
namespace CodeGen {

struct CallArg {
  RValueTy Value;
  QualType Type;

  CallArg(RValueTy value, QualType type)
    : Value(value), Type(type) {
  }
};

class CGFunctionInfo {
public:
  llvm::FunctionType *Type;
  llvm::CallingConv::ID CC;

  llvm::FunctionType *getFunctionType() const {
    return Type;
  }
  llvm::CallingConv::ID getCallingConv() const {
    return CC;
  }
};

}
} // end namespace flang

#endif
