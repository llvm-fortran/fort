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
#include "CGIORuntime.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/DeclVisitor.h"
#include "flang/Basic/Diagnostic.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/SmallString.h"
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

  // FIXME
  // Initialize the type cache.
  llvm::LLVMContext &LLVMContext = M.getContext();
  VoidTy = llvm::Type::getVoidTy(LLVMContext);
  Int8Ty = llvm::Type::getInt8Ty(LLVMContext);
  Int16Ty = llvm::Type::getInt16Ty(LLVMContext);
  Int32Ty = llvm::Type::getInt32Ty(LLVMContext);
  Int64Ty = llvm::Type::getInt64Ty(LLVMContext);
  FloatTy = llvm::Type::getFloatTy(LLVMContext);
  DoubleTy = llvm::Type::getDoubleTy(LLVMContext);
  PointerWidthInBits = 64;//C.getTargetInfo().getPointerWidth(0);
  /*PointerAlignInBytes =
  C.toCharUnitsFromBits(C.getTargetInfo().getPointerAlign(0)).getQuantity();
  IntTy = llvm::IntegerType::get(LLVMContext, C.getTargetInfo().getIntWidth());*/
  IntPtrTy = llvm::IntegerType::get(LLVMContext, PointerWidthInBits);
  Int8PtrTy = Int8Ty->getPointerTo(0);
  Int8PtrPtrTy = Int8PtrTy->getPointerTo(0);

  IORuntime = CreateLibflangIORuntime(*this);
}

CodeGenModule::~CodeGenModule() {
  if(IORuntime)
    delete IORuntime;
}

void CodeGenModule::Release() {
}

llvm::Value*
CodeGenModule::GetCFunction(StringRef Name,
                            ArrayRef<llvm::Type*> ArgTypes,
                            llvm::Type *ReturnType) {
  if(auto Func = TheModule.getFunction(Name))
    return Func;
  auto FType = llvm::FunctionType::get(ReturnType? ReturnType :
                                       VoidTy, ArgTypes,
                                       false);
  auto Func = llvm::Function::Create(FType, llvm::GlobalValue::ExternalLinkage,
                                     Name, &TheModule);
  Func->setCallingConv(llvm::CallingConv::C);
  return Func;
}

llvm::Value *
CodeGenModule::GetRuntimeFunction(StringRef Name,
                                  ArrayRef<llvm::Type*> ArgTypes,
                                  llvm::Type *ReturnType) {
  llvm::SmallString<32> MangledName("libflang_");
  MangledName.append(Name);
  return GetCFunction(MangledName, ArgTypes, ReturnType);
}

CGFunction
CodeGenModule::GetRuntimeFunction(StringRef Name,
                                  ArrayRef<CGType> ArgTypes,
                                  CGType ReturnType) {
  llvm::SmallString<32> MangledName("libflang_");
  MangledName.append(Name);

  auto SearchResult = RuntimeFunctions.find(MangledName);
  if(SearchResult != RuntimeFunctions.end())
    return SearchResult->second;

  auto FunctionInfo = Types.GetFunctionType(LibflangABI,
                                            ArgTypes,
                                            ReturnType);
  auto Func = llvm::Function::Create(FunctionInfo->getFunctionType(),
                                     llvm::GlobalValue::ExternalLinkage,
                                     llvm::Twine(MangledName), &TheModule);
  Func->setCallingConv(FunctionInfo->getCallingConv());
  auto Result = CGFunction(FunctionInfo, Func);
  RuntimeFunctions[MangledName] = Result;
  return Result;
}

CGFunction CodeGenModule::GetFunction(const FunctionDecl *Function) {
  auto SearchResult = Functions.find(Function);
  if(SearchResult != Functions.end())
    return SearchResult->second;

  auto FunctionInfo = Types.GetFunctionType(Function);

  auto Func = llvm::Function::Create(FunctionInfo->getFunctionType(),
                                     llvm::GlobalValue::ExternalLinkage,
                                     Function->getName(), &TheModule);
  Func->setCallingConv(FunctionInfo->getCallingConv());
  auto Result = CGFunction(FunctionInfo, Func);
  Functions.insert(std::make_pair(Function, Result));
  return Result;
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
  auto FuncInfo = GetFunction(Function);

  CodeGenFunction CGF(*this, FuncInfo.getFunction());
  CGF.EmitFunctionArguments(Function);
  CGF.EmitFunctionPrologue(Function, FuncInfo.getInfo());
  CGF.EmitFunctionBody(Function, Function->getBody());
  CGF.EmitFunctionEpilogue(Function, FuncInfo.getInfo());
}



}
} // end namespace flang
