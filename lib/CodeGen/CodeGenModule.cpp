//===--- CodeGenModule.cpp - Emit LLVM Code from ASTs for a Module --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This coordinates the per-module state used while generating code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenModule.h"
#include "CodeGenFunction.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/DeclVisitor.h"
#include "flang/Basic/Diagnostic.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/ConvertUTF.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Target/Mangler.h"

namespace flang {
namespace CodeGen {

CodeGenModule::CodeGenModule(ASTContext &C, const CodeGenOptions &CGO,
                             llvm::Module &M, const llvm::DataLayout &TD,
                             DiagnosticsEngine &diags)
  : Context(C), LangOpts(C.getLangOpts()), CodeGenOpts(CGO), TheModule(M),
    Diags(diags), TheDataLayout(TD), VMContext(M.getContext()), Types(*this) {

  // Initialize the type cache.
  llvm::LLVMContext &LLVMContext = M.getContext();
  VoidTy = llvm::Type::getVoidTy(LLVMContext);
  Int8Ty = llvm::Type::getInt8Ty(LLVMContext);
  Int16Ty = llvm::Type::getInt16Ty(LLVMContext);
  Int32Ty = llvm::Type::getInt32Ty(LLVMContext);
  Int64Ty = llvm::Type::getInt64Ty(LLVMContext);
  FloatTy = llvm::Type::getFloatTy(LLVMContext);
  DoubleTy = llvm::Type::getDoubleTy(LLVMContext);
  /*PointerWidthInBits = C.getTargetInfo().getPointerWidth(0);
  PointerAlignInBytes =
  C.toCharUnitsFromBits(C.getTargetInfo().getPointerAlign(0)).getQuantity();
  IntTy = llvm::IntegerType::get(LLVMContext, C.getTargetInfo().getIntWidth());
  IntPtrTy = llvm::IntegerType::get(LLVMContext, PointerWidthInBits);*/
  Int8PtrTy = Int8Ty->getPointerTo(0);
  Int8PtrPtrTy = Int8PtrTy->getPointerTo(0);
}

CodeGenModule::~CodeGenModule() {
}


void CodeGenModule::Release() {
}

void CodeGenModule::EmitTopLevelDecl(const Decl *Declaration) {
  class Visitor : public ConstDeclVisitor<Visitor> {
  public:
    CodeGenModule *CG;

    Visitor(CodeGenModule *P) : CG(P) {}

    void VisitMainProgramDecl(const MainProgramDecl *D) {
      CG->EmitMainProgramDecl(D);
    }
    void VisitFunctionDecl(const FunctionDecl *D) {
      CG->EmitFunctionDecl(D);
    }
  };
  Visitor DV(this);
  DV.Visit(Declaration);
}

void CodeGenModule::EmitMainProgramDecl(const MainProgramDecl *Program) {
  auto FType = llvm::FunctionType::get(Int32Ty, false);
  auto Linkage = llvm::GlobalValue::ExternalLinkage;
  auto Func = llvm::Function::Create(FType, Linkage, "main", &TheModule);
  Func->setCallingConv(llvm::CallingConv::C);

  CodeGenFunction CGF(*this, Func);
  CGF.EmitMainProgramBody(Program, Program->getBody());
}

void CodeGenModule::EmitFunctionDecl(const FunctionDecl *Function) {

}


}
} // end namespace flang
