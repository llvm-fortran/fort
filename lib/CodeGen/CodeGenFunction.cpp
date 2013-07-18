//===--- CodeGenFunction.cpp - Emit LLVM Code from ASTs for a Function ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This coordinates the per-function state used while generating code.
//
//===----------------------------------------------------------------------===//

#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/ASTContext.h"
#include "flang/AST/Decl.h"
#include "flang/AST/DeclVisitor.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/Expr.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Operator.h"

namespace flang {
namespace CodeGen {

CodeGenFunction::CodeGenFunction(CodeGenModule &cgm, llvm::Function *Fn)
  : CGM(cgm), /*, Target(cgm.getTarget()),*/
    Builder(cgm.getModule().getContext()),
    UnreachableBlock(nullptr), CurFn(Fn), IsMainProgram(false),
    ReturnValuePtr(nullptr) {
}

CodeGenFunction::~CodeGenFunction() {
}

void CodeGenFunction::EmitFunctionDecls(const DeclContext *DC) {
  class Visitor : public ConstDeclVisitor<Visitor> {
  public:
    CodeGenFunction *CG;

    Visitor(CodeGenFunction *P) : CG(P) {}

    void VisitVarDecl(const VarDecl *D) {
      CG->EmitVarDecl(D);
    }
  };
  Visitor DV(this);
  DV.Visit(DC);
}

void CodeGenFunction::EmitMainProgramBody(const DeclContext *DC, const Stmt *S) {
  auto Block = createBasicBlock("program_entry",getCurrentFunction());
  Builder.SetInsertPoint(Block);
  IsMainProgram = true;

  EmitFunctionDecls(DC);

  ReturnBlock = createBasicBlock("program_exit",getCurrentFunction());
  if(S) {
    EmitStmt(S);
  }

  Block = Builder.GetInsertBlock();
  ReturnBlock->moveAfter(Block);
  //if(!isa<llvm::BranchInst>(Block->getTerminator())) {
    Builder.CreateBr(ReturnBlock);
  //}
  Builder.SetInsertPoint(ReturnBlock);
  auto ReturnValue = Builder.getInt32(0);
  Builder.CreateRet(ReturnValue);
}

void CodeGenFunction::EmitFunctionArguments(const FunctionDecl *Func,
                                            const CGFunctionInfo *Info) {
  ArgsList = Func->getArguments();
  ArgsInfo = Info->getArguments();

  size_t I = 0;
  auto Arg = CurFn->arg_begin();

  for(; I < ArgsList.size(); ++Arg, ++I) {
    auto ArgDecl = ArgsList[I];
    auto Info = GetArgInfo(ArgDecl);

    if(Info.ABIInfo.getKind() == ABIArgInfo::ExpandCharacterPutLengthToAdditionalArgsAsInt) {
      ExpandedArg AArg;
      AArg.Decl = ArgDecl;
      AArg.A1 = Arg;
      ExpandedArgs.push_back(AArg);
    }
    else
      LocalVariables.insert(std::make_pair(ArgDecl, Arg));

    Arg->setName(ArgDecl->getName());
  }

  // Extra argument for the returned data.
  if(Info->getReturnInfo().ABIInfo.getKind() == ABIRetInfo::CharacterValueAsArg) {
    Arg->setName(Func->getName());
    ReturnValuePtr = Arg;
    ++Arg;
  }

  // Additional arguments.
  for(I = 0; I < ExpandedArgs.size(); ++Arg, ++I) {
    Arg->setName(llvm::Twine(ExpandedArgs[I].Decl->getName()) + ".length");
    ExpandedArgs[I].A2 = Arg;
  }
}

void CodeGenFunction::EmitFunctionPrologue(const FunctionDecl *Func,
                                           const CGFunctionInfo *Info) {
  EmitBlock(createBasicBlock("entry"));
  auto RetABI = Info->getReturnInfo().ABIInfo.getKind();
  if(RetABI == ABIRetInfo::Value) {
    ReturnValuePtr = Builder.CreateAlloca(ConvertType(Func->getType()),
                                          nullptr, Func->getName());
  }
  ReturnBlock = createBasicBlock("return");
}

void CodeGenFunction::EmitFunctionBody(const DeclContext *DC, const Stmt *S) {
  EmitFunctionDecls(DC);
  if(S)
    EmitStmt(S);
}

void CodeGenFunction::EmitFunctionEpilogue(const FunctionDecl *Func,
                                           const CGFunctionInfo *Info) {
  EmitBlock(ReturnBlock);
  if(auto RetVar = GetRetVarPtr()) {
    auto ReturnInfo = Info->getReturnInfo();

    if(ReturnInfo.ABIInfo.getKind() == ABIRetInfo::Value) {
      switch(ReturnInfo.Kind) {
      case CGFunctionInfo::RetInfo::ScalarValue:
        Builder.CreateRet(Builder.CreateLoad(RetVar));
        break;
      case CGFunctionInfo::RetInfo::ComplexValue:
        Builder.CreateRet(CreateComplexAggregate(
                            EmitComplexLoad(RetVar)));
        break;
      }
    }
    else Builder.CreateRetVoid();
  } else
    Builder.CreateRetVoid();
}

void CodeGenFunction::EmitVarDecl(const VarDecl *D) {
  if(D->isParameter() ||
     D->isArgument()) return;

  auto Type = D->getType();
  llvm::Value *Ptr;
  if(Type->isArrayType())
    Ptr = CreateArrayAlloca(Type, D->getName());
  else Ptr = Builder.CreateAlloca(ConvertTypeForMem(Type),
                                  nullptr, D->getName());
  LocalVariables.insert(std::make_pair(D, Ptr));
}

llvm::Value *CodeGenFunction::GetVarPtr(const VarDecl *D) {
  return LocalVariables[D];
}

llvm::Value *CodeGenFunction::GetRetVarPtr() {
  return ReturnValuePtr;
}

CGFunctionInfo::ArgInfo CodeGenFunction::GetArgInfo(const VarDecl *Arg) const {
  for(size_t I = 0; I < ArgsList.size(); ++I) {
    if(ArgsList[I] == Arg) return ArgsInfo[I];
  }
  assert(false && "Invalid argument");
  return CGFunctionInfo::ArgInfo();
}

llvm::Value *CodeGenFunction::GetIntrinsicFunction(int FuncID,
                                                   ArrayRef<llvm::Type*> ArgTypes) const {
  return llvm::Intrinsic::getDeclaration(&CGM.getModule(),
                                         (llvm::Intrinsic::ID)FuncID,
                                         ArgTypes);
}

llvm::Value *CodeGenFunction::GetIntrinsicFunction(int FuncID,
                                                   llvm::Type *T1) const {
  return llvm::Intrinsic::getDeclaration(&CGM.getModule(),
                                         (llvm::Intrinsic::ID)FuncID,
                                         T1);
}

llvm::Value *CodeGenFunction::GetIntrinsicFunction(int FuncID,
                                                   llvm::Type *T1, llvm::Type *T2) const {
  llvm::Type *ArgTypes[2] = {T1, T2};
  return llvm::Intrinsic::getDeclaration(&CGM.getModule(),
                                         (llvm::Intrinsic::ID)FuncID,
                                         ArrayRef<llvm::Type*>(ArgTypes,2));
}

llvm::Type *CodeGenFunction::ConvertTypeForMem(QualType T) const {
  return CGM.getTypes().ConvertTypeForMem(T);
}

llvm::Type *CodeGenFunction::ConvertType(QualType T) const {
  return CGM.getTypes().ConvertType(T);
}

} // end namespace CodeGen
} // end namespace flang
