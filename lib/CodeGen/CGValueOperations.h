//===-- CGValueOperations.h - Generic wrappers for core operations on values =//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_CGVALUEOPERATONS_H
#define FLANG_CODEGEN_CGVALUEOPERATONS_H

#include "CGValue.h"

namespace flang {
namespace CodeGen {

class CodeGenFunction;

template<typename T>
struct ValueOperations {};

/// Operations for scalar values.
template<>
struct ValueOperations<llvm::Value*> {
  static llvm::Value* EmitLoad(CodeGenFunction &CGF, llvm::Value *Ptr, bool IsVolatile = false) {
    return CGF.getBuilder().CreateLoad(Ptr, IsVolatile);
  }

  static void EmitStore(CodeGenFunction &CGF, llvm::Value *Val, llvm::Value *Ptr, bool IsVolatile = false) {
    CGF.getBuilder().CreateStore(Val, Ptr, IsVolatile);
  }
};

/// Operations for complex values.
template<>
struct ValueOperations<ComplexValueTy> {

  static ComplexValueTy EmitLoad(CodeGenFunction &CGF, llvm::Value *Ptr, bool IsVolatile = false) {
    return CGF.EmitComplexLoad(Ptr, IsVolatile);
  }

  static void EmitStore(CodeGenFunction &CGF, ComplexValueTy Val, llvm::Value *Ptr, bool IsVolatile = false) {
    CGF.EmitComplexStore(Val, Ptr, IsVolatile);
  }
};

}
}

#endif
