//===----- CGSystemLibfort.cpp - Interface to Libfort System Runtime -----===//
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

namespace fort {
namespace CodeGen {

class CGLibfortSystemRuntime : public CGSystemRuntime {
public:
  CGLibfortSystemRuntime(CodeGenModule &CGM)
    : CGSystemRuntime(CGM) {
  }

  void EmitInit(CodeGenFunction &CGF);

  llvm::Value *EmitMalloc(CodeGenFunction &CGF, llvm::Value *Size);
  void EmitFree(CodeGenFunction &CGF, llvm::Value *Ptr);

  llvm::Value *EmitETIME(CodeGenFunction &CGF, ArrayRef<Expr*> Arguments);
};

void CGLibfortSystemRuntime::EmitInit(CodeGenFunction &CGF) {
  auto Func = CGM.GetRuntimeFunction("sys_init", ArrayRef<CGType>());
  CallArgList ArgList;
  CGF.EmitCall(Func.getFunction(), Func.getInfo(), ArgList);
}

llvm::Value *CGLibfortSystemRuntime::EmitMalloc(CodeGenFunction &CGF, llvm::Value *Size) {
  auto Func = CGM.GetRuntimeFunction1("malloc", CGM.SizeTy, CGM.VoidPtrTy);
  CallArgList ArgList;
  CGF.EmitCallArg(ArgList, Size, Func.getInfo()->getArguments()[0]);
  return CGF.EmitCall(Func.getFunction(), Func.getInfo(), ArgList).asScalar();
}

void CGLibfortSystemRuntime::EmitFree(CodeGenFunction &CGF, llvm::Value *Ptr) {
  auto Func = CGM.GetRuntimeFunction1("free", CGM.VoidPtrTy);
  CallArgList ArgList;
  CGF.EmitCallArg(ArgList, Ptr->getType() == CGM.VoidPtrTy?
                           Ptr : CGF.getBuilder().CreateBitCast(Ptr, CGM.VoidPtrTy),
                  Func.getInfo()->getArguments()[0]);
  CGF.EmitCall(Func.getFunction(), Func.getInfo(), ArgList);
}

llvm::Value *CGLibfortSystemRuntime::EmitETIME(CodeGenFunction &CGF, ArrayRef<Expr*> Arguments) {
  auto RealTy = CGM.getContext().RealTy;
  auto RealPtrTy = llvm::PointerType::get(CGF.ConvertTypeForMem(RealTy) ,0);
  auto Func = CGM.GetRuntimeFunction2(
    RealTy->getBuiltinTypeKind() == BuiltinType::Real4? "etimef" : "etime",
                                      RealPtrTy, RealPtrTy, RealTy);
  auto Arr = CGF.EmitArrayArgumentPointerValueABI(Arguments[0]);
  CallArgList ArgList;
  CGF.EmitCallArg(ArgList, Arr, Func.getInfo()->getArguments()[0]);
  CGF.EmitCallArg(ArgList,
                  CGF.getBuilder().CreateConstInBoundsGEP1_32(
                    CGF.ConvertTypeForMem(RealTy),Arr, 1),
                  Func.getInfo()->getArguments()[1]);
  return CGF.EmitCall(Func.getFunction(), Func.getInfo(), ArgList).asScalar();
}

CGSystemRuntime *CreateLibfortSystemRuntime(CodeGenModule &CGM) {
  return new CGLibfortSystemRuntime(CGM);
}

}
} // end namespace fort

