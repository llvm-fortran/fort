//===-- CodeGenFunction.h - Per-Function state for LLVM CodeGen -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the internal per-function state used for llvm translation.
//
//===----------------------------------------------------------------------===//

#ifndef CLANG_CODEGEN_CODEGENFUNCTION_H
#define CLANG_CODEGEN_CODEGENFUNCTION_H

#include "CGBuilder.h"
#include "CGValue.h"
#include "CodeGenModule.h"
#include "flang/AST/Expr.h"
#include "flang/AST/Stmt.h"
#include "flang/AST/Type.h"
//#include "flang/Basic/TargetInfo.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ValueHandle.h"

namespace llvm {
  class BasicBlock;
  class LLVMContext;
  class MDNode;
  class Module;
  class SwitchInst;
  class Twine;
  class Value;
  class CallSite;
}

namespace flang {
  class ASTContext;
  class Decl;

namespace CodeGen {
  class CodeGenTypes;

/// CodeGenFunction - This class organizes the per-function state that is used
/// while generating LLVM code.
class CodeGenFunction : public CodeGenTypeCache {
  CodeGenFunction(const CodeGenFunction &) LLVM_DELETED_FUNCTION;
  void operator=(const CodeGenFunction &) LLVM_DELETED_FUNCTION;

  CodeGenModule &CGM;  // Per-module state.
  //const TargetInfo &Target;

  CGBuilderTy Builder;

  /// CurFuncDecl - Holds the Decl for the current outermost
  /// non-closure context.
  const Decl *CurFuncDecl;
  /// CurCodeDecl - This is the inner-most code context, which includes blocks.
  const Decl *CurCodeDecl;
  QualType FnRetTy;
  llvm::Function *CurFn;

  llvm::BasicBlock *UnreachableBlock;

  llvm::BasicBlock *ReturnBlock;

  llvm::DenseMap<const Stmt*, llvm::BasicBlock*> GotoTargets;

  llvm::DenseMap<const VarDecl*, llvm::Value*> LocalVariables;

  RValueTy ReturnValuePtr;

  bool IsMainProgram;

public:
  CodeGenFunction(CodeGenModule &cgm, llvm::Function *Fn);
  ~CodeGenFunction();

  //CodeGenTypes &getTypes() const { return CGM.getTypes(); }
  ASTContext &getContext() const { return CGM.getContext(); }

  CGBuilderTy &getBuilder() { return Builder; }

  llvm::BasicBlock *getUnreachableBlock() {
    if (!UnreachableBlock) {
      UnreachableBlock = createBasicBlock("unreachable");
      new llvm::UnreachableInst(getLLVMContext(), UnreachableBlock);
    }
    return UnreachableBlock;
  }
  //const TargetInfo &getTarget() const { return Target; }
  llvm::LLVMContext &getLLVMContext() { return CGM.getLLVMContext(); }

  llvm::Function *getCurrentFunction() const {
    return CurFn;
  }

  /// EmitReturnBlock - Emit the unified return block, trying to avoid its
  /// emission when possible.
  void EmitReturnBlock();

  llvm::Type *ConvertTypeForMem(QualType T) const;
  llvm::Type *ConvertType(QualType T) const;

  /// createBasicBlock - Create an LLVM basic block.
  llvm::BasicBlock *createBasicBlock(const Twine &name = "",
                                     llvm::Function *parent = 0,
                                     llvm::BasicBlock *before = 0) {
#ifdef NDEBUG
    return llvm::BasicBlock::Create(getLLVMContext(), "", parent, before);
#else
    return llvm::BasicBlock::Create(getLLVMContext(), name, parent, before);
#endif
  }

  llvm::Value *GetIntrinsicFunction(int FuncID,
                                    ArrayRef<llvm::Type*> ArgTypes) const;
  llvm::Value *GetIntrinsicFunction(int FuncID,
                                    llvm::Type *T1) const;
  llvm::Value *GetIntrinsicFunction(int FuncID,
                                    llvm::Type *T1, llvm::Type *T2) const;
  inline
  llvm::Value *GetIntrinsicFunction(int FuncID, QualType T1) const {
    return GetIntrinsicFunction(FuncID, ConvertType(T1));
  }
  inline
  llvm::Value *GetIntrinsicFunction(int FuncID,
                                    QualType T1, QualType T2) const {
    return GetIntrinsicFunction(FuncID, ConvertType(T1), ConvertType(T2));
  }

  llvm::Value *GetVarPtr(const VarDecl *D);

  void EmitFunctionDecls(const DeclContext *DC);
  void EmitMainProgramBody(const DeclContext *DC, const Stmt *S);
  void EmitFunctionBody(const DeclContext *DC, const Stmt *S);

  void EmitReturnVarDecl(const ReturnVarDecl *D);
  void EmitVarDecl(const VarDecl *D);

  void EmitBlock(llvm::BasicBlock *BB);
  void EmitBranch(llvm::BasicBlock *Target);
  void EmitBranchOnLogicalExpr(const Expr *Condition, llvm::BasicBlock *ThenBB,
                               llvm::BasicBlock *ElseBB);

  void EmitStmt(const Stmt *S);
  void EmitStmtLabel(const Stmt *S);

  void EmitGotoStmt(const GotoStmt *S);
  void EmitIfStmt(const IfStmt *S);
  void EmitDoStmt(const DoStmt *S);
  void EmitDoWhileStmt(const DoWhileStmt *S);
  void EmitStopStmt(const StopStmt *S);
  void EmitReturnStmt(const ReturnStmt *S);
  void EmitAssignmentStmt(const AssignmentStmt *S);
  void EmitAssignment(const Expr *LHS, const Expr *RHS);

  LValueTy EmitLValue(const Expr *E);

  llvm::Value *EmitScalarRValue(const Expr *E);
  ComplexValueTy EmitComplexRValue(const Expr *E);
  llvm::Value *EmitScalarExpr(const Expr *E);
  llvm::Value *EmitIntToInt32Conversion(llvm::Value *Value);
  llvm::Value *EmitScalarToScalarConversion(llvm::Value *Value, QualType Target);
  llvm::Value *EmitLogicalScalarExpr(const Expr *E);
  llvm::Value *EmitIntegerConstantExpr(const IntegerConstantExpr *E);
  llvm::Value *EmitScalarRelationalExpr(BinaryExpr::Operator Op, llvm::Value *LHS,
                                        llvm::Value *RHS);

  llvm::Value *GetConstantZero(QualType T);
  llvm::Value *GetConstantOne(QualType T);

  ComplexValueTy EmitComplexExpr(const Expr *E);
  ComplexValueTy EmitComplexLoad(llvm::Value *Ptr, bool IsVolatile = false);
  void EmitComplexStore(ComplexValueTy Value, llvm::Value *Ptr,
                        bool IsVolatile = false);
  ComplexValueTy EmitComplexToComplexConversion(ComplexValueTy Value, QualType Target);
  ComplexValueTy EmitScalarToComplexConversion(llvm::Value *Value, QualType Target);
  llvm::Value *EmitComplexToScalarConversion(ComplexValueTy Value, QualType Target);
  llvm::Value *EmitComplexRelationalExpr(BinaryExpr::Operator Op, ComplexValueTy LHS,
                                         ComplexValueTy RHS);
  ComplexValueTy EmitComplexToPolarFormConversion(ComplexValueTy Value);

};

}  // end namespace CodeGen
}  // end namespace flang

#endif
