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
class CodeGenFunction {
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

  llvm::Value *ReturnValuePtr;

  bool IsMainProgram;

public:
  CodeGenFunction(CodeGenModule &cgm, llvm::Function *Fn);
  ~CodeGenFunction();

  CodeGenModule &getModule() const {
    return CGM;
  }

  CodeGenTypes &getTypes() const {
    return CGM.getTypes();
  }

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
  llvm::Value *GetRetVarPtr();

  void EmitFunctionDecls(const DeclContext *DC);
  void EmitMainProgramBody(const DeclContext *DC, const Stmt *S);
  void EmitFunctionArguments(const FunctionDecl *Func);
  void EmitFunctionPrologue(const FunctionDecl *Func);
  void EmitFunctionBody(const DeclContext *DC, const Stmt *S);
  void EmitFunctionEpilogue(const FunctionDecl *Func);

  void EmitVarDecl(const VarDecl *D);

  /// CreateTempAlloca - This creates a alloca and inserts it into the entry
  /// block. The caller is responsible for setting an appropriate alignment on
  /// the alloca.
  llvm::AllocaInst *CreateTempAlloca(llvm::Type *Ty,
                                     const llvm::Twine &Name = "tmp");

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
  void EmitCallStmt(const CallStmt *S);
  void EmitAssignmentStmt(const AssignmentStmt *S);
  void EmitAssignment(const Expr *LHS, const Expr *RHS);
  void EmitAssignment(LValueTy LHS, RValueTy RHS);

  RValueTy EmitRValue(const Expr *E);
  LValueTy EmitLValue(const Expr *E);

  llvm::Value *EmitScalarExpr(const Expr *E);
  llvm::Value *EmitIntToInt32Conversion(llvm::Value *Value);
  llvm::Value *EmitScalarToScalarConversion(llvm::Value *Value, QualType Target);
  llvm::Value *EmitLogicalScalarExpr(const Expr *E);
  llvm::Value *EmitIntegerConstantExpr(const IntegerConstantExpr *E);
  llvm::Value *EmitScalarRelationalExpr(BinaryExpr::Operator Op, llvm::Value *LHS,
                                        llvm::Value *RHS);

  llvm::Value *GetConstantZero(QualType T);
  llvm::Value *GetConstantOne(QualType T);

  ComplexValueTy ExtractComplexValue(llvm::Value *Agg);
  llvm::Value   *CreateComplexAggregate(ComplexValueTy Value);
  ComplexValueTy EmitComplexExpr(const Expr *E);
  ComplexValueTy EmitComplexLoad(llvm::Value *Ptr, bool IsVolatile = false);
  void EmitComplexStore(ComplexValueTy Value, llvm::Value *Ptr,
                        bool IsVolatile = false);
  ComplexValueTy EmitComplexBinaryExpr(BinaryExpr::Operator Op, ComplexValueTy LHS,
                                       ComplexValueTy RHS);
  ComplexValueTy EmitComplexToComplexConversion(ComplexValueTy Value, QualType Target);
  ComplexValueTy EmitScalarToComplexConversion(llvm::Value *Value, QualType Target);
  llvm::Value *EmitComplexToScalarConversion(ComplexValueTy Value, QualType Target);
  llvm::Value *EmitComplexRelationalExpr(BinaryExpr::Operator Op, ComplexValueTy LHS,
                                         ComplexValueTy RHS);
  ComplexValueTy EmitComplexToPolarFormConversion(ComplexValueTy Value);

  RValueTy EmitIntrinsicCall(const IntrinsicCallExpr *E);
  llvm::Value *EmitIntrinsicCallScalarTruncation(intrinsic::FunctionKind Func,
                                                 llvm::Value *Value,
                                                 QualType ResultType);
  RValueTy EmitIntrinsicCallComplex(intrinsic::FunctionKind Func, ComplexValueTy Value);
  llvm::Value* EmitIntrinsicCallScalarMath(intrinsic::FunctionKind Func,
                                           llvm::Value *A1, llvm::Value *A2 = nullptr);
  ComplexValueTy EmitIntrinsicCallComplexMath(intrinsic::FunctionKind Func,
                                              ComplexValueTy Value);

  RValueTy EmitCall(const CallExpr *E);
  RValueTy EmitCall(const FunctionDecl *Function, ArrayRef<Expr *> Arguments,
                    bool ReturnsNothing = false);
  RValueTy EmitCall(CGFunctionInfo FuncInfo, ArrayRef<Expr *> Arguments,
                    QualType ReturnType, bool ReturnsNothing = false);
  llvm::Value *EmitCallArgPtr(const Expr *E);


};

}  // end namespace CodeGen
}  // end namespace flang

#endif
