//===----- CGSystemLibflang.cpp - Interface to Libflang System Runtime -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "CGSystemRuntime.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"

namespace flang {
namespace CodeGen {

class CGLibflangSystemRuntime : public CGSystemRuntime {
public:
  CGLibflangSystemRuntime(CodeGenModule &CGM)
    : CGSystemRuntime(CGM) {
  }

  llvm::Value *EmitMalloc(CodeGenFunction &CGF, llvm::Value *Size);
  void EmitFree(CodeGenFunction &CGF, llvm::Value *Ptr);
};

llvm::Value *CGLibflangSystemRuntime::EmitMalloc(CodeGenFunction &CGF, llvm::Value *Size) {
  auto Func = CGM.GetRuntimeFunction1("malloc", CGM.SizeTy, CGM.VoidPtrTy);
  CallArgList ArgList;
  CGF.EmitCallArg(ArgList, Size, Func.getInfo()->getArguments()[0]);
  return CGF.EmitCall(Func.getFunction(), Func.getInfo(), ArgList).asScalar();
}

void CGLibflangSystemRuntime::EmitFree(CodeGenFunction &CGF, llvm::Value *Ptr) {
  auto Func = CGM.GetRuntimeFunction1("free", CGM.VoidPtrTy);
  CallArgList ArgList;
  CGF.EmitCallArg(ArgList, Ptr->getType() == CGM.VoidPtrTy?
                           Ptr : CGF.getBuilder().CreateBitCast(Ptr, CGM.VoidPtrTy),
                  Func.getInfo()->getArguments()[0]);
  CGF.EmitCall(Func.getFunction(), Func.getInfo(), ArgList);
}

CGSystemRuntime *CreateLibflangSystemRuntime(CodeGenModule &CGM) {
  return new CGLibflangSystemRuntime(CGM);
}

}
} // end namespace flang

