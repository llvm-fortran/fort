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
    UnreachableBlock(nullptr), CurFn(Fn), IsMainProgram(false) {
}

CodeGenFunction::~CodeGenFunction() {
}

void CodeGenFunction::EmitFunctionDecls(const DeclContext *DC) {
}

void CodeGenFunction::EmitMainProgramBody(const Stmt *S) {
  auto Block = createBasicBlock("program_entry",getCurrentFunction());
  Builder.SetInsertPoint(Block);
  ReturnBlock = createBasicBlock("program_exit",getCurrentFunction());

  IsMainProgram = true;
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

void CodeGenFunction::EmitFunctionBody(const Stmt *S) {

}


llvm::Type *CodeGenFunction::ConvertTypeForMem(QualType T) {
  return nullptr;
  //return CGM.getTypes().ConvertTypeForMem(T);
}

llvm::Type *CodeGenFunction::ConvertType(QualType T) {
  return nullptr;
  //return CGM.getTypes().ConvertType(T);
}

} // end namespace CodeGen
} // end namespace flang
