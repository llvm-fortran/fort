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
#include "CGIORuntime.h"
#include "CGSystemRuntime.h"
#include "CodeGenFunction.h"
#include "fort/AST/ASTContext.h"
#include "fort/AST/Decl.h"
#include "fort/AST/DeclVisitor.h"
#include "fort/Basic/Diagnostic.h"
#include "fort/Frontend/CodeGenOptions.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Mangler.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ConvertUTF.h"
#include "llvm/Support/ErrorHandling.h"

namespace fort {
namespace CodeGen {

CodeGenModule::CodeGenModule(ASTContext &C, const CodeGenOptions &CGO,
                             llvm::Module &M, const llvm::DataLayout &TD,
                             DiagnosticsEngine &diags)
    : Context(C), LangOpts(C.getLangOpts()), CodeGenOpts(CGO), TheModule(M),
      Diags(diags), TheDataLayout(TD), VMContext(M.getContext()), Types(*this),
      TheTargetCodeGenInfo(nullptr) {

  llvm::LLVMContext &LLVMContext = M.getContext();
  VoidTy = llvm::Type::getVoidTy(LLVMContext);
  Int1Ty = llvm::Type::getInt1Ty(LLVMContext);
  Int8Ty = llvm::Type::getInt8Ty(LLVMContext);
  Int16Ty = llvm::Type::getInt16Ty(LLVMContext);
  Int32Ty = llvm::Type::getInt32Ty(LLVMContext);
  Int64Ty = llvm::Type::getInt64Ty(LLVMContext);
  FloatTy = llvm::Type::getFloatTy(LLVMContext);
  DoubleTy = llvm::Type::getDoubleTy(LLVMContext);
  PointerWidthInBits = TD.getPointerSizeInBits(0);
  IntPtrTy = llvm::IntegerType::get(LLVMContext, PointerWidthInBits);
  Int8PtrTy = Int8Ty->getPointerTo(0);
  Int8PtrPtrTy = Int8PtrTy->getPointerTo(0);
  RuntimeCC = llvm::CallingConv::C;

  IORuntime = CreateLibfortIORuntime(*this);
  SystemRuntime = CreateLibfortSystemRuntime(*this);
}

CodeGenModule::~CodeGenModule() {
  if (IORuntime)
    delete IORuntime;
}

void CodeGenModule::Release() {}

llvm::Value *CodeGenModule::GetCFunction(StringRef Name,
                                         ArrayRef<llvm::Type *> ArgTypes,
                                         llvm::Type *ReturnType) {
  if (auto Func = TheModule.getFunction(Name))
    return Func;
  auto FType = llvm::FunctionType::get(ReturnType ? ReturnType : VoidTy,
                                       ArgTypes, false);
  auto Func = llvm::Function::Create(FType, llvm::GlobalValue::ExternalLinkage,
                                     Name, &TheModule);
  Func->setCallingConv(llvm::CallingConv::C);
  return Func;
}

llvm::Value *CodeGenModule::GetRuntimeFunction(StringRef Name,
                                               ArrayRef<llvm::Type *> ArgTypes,
                                               llvm::Type *ReturnType) {
  llvm::SmallString<32> MangledName("libfort_");
  MangledName.append(Name);
  return GetCFunction(MangledName, ArgTypes, ReturnType);
}

CGFunction CodeGenModule::GetRuntimeFunction(StringRef Name,
                                             ArrayRef<CGType> ArgTypes,
                                             CGType ReturnType,
                                             FortranABI *ABI) {
  llvm::SmallString<32> MangledName("libfort_");
  MangledName.append(Name);

  auto SearchResult = RuntimeFunctions.find(MangledName);
  if (SearchResult != RuntimeFunctions.end())
    return SearchResult->second;

  auto FunctionInfo =
      Types.GetFunctionType(ABI ? *ABI : RuntimeABI, ArgTypes, ReturnType);
  auto Func = llvm::Function::Create(FunctionInfo->getFunctionType(),
                                     llvm::GlobalValue::ExternalLinkage,
                                     llvm::Twine(MangledName), &TheModule);
  Func->setCallingConv(FunctionInfo->getCallingConv());
  auto Result = CGFunction(FunctionInfo, Func);
  RuntimeFunctions[MangledName] = Result;
  return Result;
}

/// Mangle name of a Fortran symbol inside of a module
static std::string FortranNameMangle(StringRef ModuleName, StringRef SymName) {
  // TODO may be consolidate name operations in one class
  llvm::SmallString<256> Buffer;
  llvm::raw_svector_ostream Out(Buffer);
  Out << "__" << ModuleName << "_MOD_" << SymName << "_";

  return Out.str();
}

/// Produce IR function definition for a Fortran function
/// using GNU Fortran conventions for name mangling
CGFunction CodeGenModule::GetFunction(const FunctionDecl *Function) {
  auto SearchResult = Functions.find(Function);
  if (SearchResult != Functions.end())
    return SearchResult->second;

  auto FunctionInfo = Types.GetFunctionType(Function);
  auto DC = Function->getDeclContext();
  std::string NameStr;

  // Module name is prepended to function names defined inside of it
  if (DC && DC->isModule()) {
    auto Module = ModuleDecl::castFromDeclContext(DC);
    NameStr = FortranNameMangle(Module->getName(), Function->getName());
  } else {
    NameStr = std::string(Function->getName()) + "_";
  }

  StringRef FunctionName(NameStr);
  auto Func = llvm::Function::Create(FunctionInfo->getFunctionType(),
                                     llvm::GlobalValue::ExternalLinkage,
                                     FunctionName, &TheModule);

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
    void VisitFunctionDecl(const FunctionDecl *D) { CG->EmitFunctionDecl(D); }
    void VisitModuleDecl(const ModuleDecl *D) { CG->EmitModuleDecl(D); }
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
  CGF.EmitFunctionArguments(Function, FuncInfo.getInfo());
  CGF.EmitFunctionPrologue(Function, FuncInfo.getInfo());
  CGF.EmitFunctionBody(Function, Function->getBody());
  CGF.EmitFunctionEpilogue(Function, FuncInfo.getInfo());
}

void CodeGenModule::EmitModuleDecl(const ModuleDecl *Module) {
  class ModuleDeclVisitor : public ConstDeclVisitor<ModuleDeclVisitor> {
  public:
    CodeGenModule *CG;
    StringRef ModName;

    ModuleDeclVisitor(CodeGenModule *P, StringRef N) : CG(P), ModName(N) {}

    void VisitFunctionDecl(const FunctionDecl *D) { CG->EmitFunctionDecl(D); }
    void VisitVarDecl(const VarDecl *D) {
      StringRef Name = FortranNameMangle(ModName, D->getName());
      auto Type = CG->getTypes().ConvertTypeForMem(D->getType());

      // FIXME implement constant initializers
      llvm::Constant *Initializer = nullptr;
      auto Init = D->getInit();

      CG->EmitGlobalVariable(Name, Type, Initializer);
    }
  };

  ModuleDeclVisitor MV(this, Module->getName());

  // Visit module declarations
  auto I = Module->decls_begin();
  for (auto E = Module->decls_end(); I != E; ++I) {
    if ((*I)->getDeclContext() == Module)
      MV.Visit(*I);
  }
}

llvm::GlobalVariable *
CodeGenModule::EmitSaveVariable(StringRef FuncName, const VarDecl *Var,
                                llvm::Constant *Initializer) {
  auto T = getTypes().ConvertTypeForMem(Var->getType());
  return EmitSaveVariable(FuncName, Var->getName(), T, Initializer);
}

llvm::GlobalVariable *
CodeGenModule::EmitSaveVariable(StringRef FuncName, StringRef VarName,
                                llvm::Type *Type, llvm::Constant *Initializer) {
  return EmitGlobalVariable(llvm::Twine(FuncName) + VarName + "_", Type,
                            Initializer);
}

llvm::GlobalVariable *
CodeGenModule::EmitGlobalVariable(llvm::Twine Name, llvm::Type *Type,
                                  llvm::Constant *Initializer) {
  return new llvm::GlobalVariable(TheModule, Type, false,
                                  llvm::GlobalValue::InternalLinkage,
                                  Initializer, Name);
}

llvm::Value *CodeGenModule::EmitConstantArray(llvm::Constant *Array) {
  // FIXME: fold identical values
  return new llvm::GlobalVariable(TheModule, Array->getType(), true,
                                  llvm::GlobalValue::PrivateLinkage, Array);
}

llvm::Value *CodeGenModule::EmitCommonBlock(const CommonBlockDecl *CB,
                                            llvm::Type *Type,
                                            llvm::Constant *Initializer) {
  llvm::SmallString<32> Name;
  StringRef NameRef;
  if (CB->getIdentifier()) {
    Name.append(CB->getName());
    Name.push_back('_');
    NameRef = Name;
  } else
    NameRef = "__BLNK__"; // FIXME?

  auto Var = TheModule.getGlobalVariable(NameRef);
  if (Var)
    return Var;
  if (!Initializer)
    Initializer = llvm::Constant::getNullValue(Type);
  auto CBVar = new llvm::GlobalVariable(TheModule, Type, false,
                                        llvm::GlobalValue::CommonLinkage,
                                        Initializer, NameRef);
  CBVar->setAlignment(16); // FIXME: proper target dependent alignment value
  return CBVar;
}

} // namespace CodeGen
} // end namespace fort
