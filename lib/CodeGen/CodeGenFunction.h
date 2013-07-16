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
  void EmitFunctionPrologue(const FunctionDecl *Func,
                            const CGFunctionInfo *Info);
  void EmitFunctionBody(const DeclContext *DC, const Stmt *S);
  void EmitFunctionEpilogue(const FunctionDecl *Func,
                            const CGFunctionInfo *Info);

  void EmitVarDecl(const VarDecl *D);

  llvm::Value *CreateArrayAlloca(QualType T,
                                 const llvm::Twine &Name = "",
                                 bool IsTemp = false);

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

  // scalar expressions.
  llvm::Value *EmitSizeIntExpr(const Expr *E);
  llvm::Value *EmitScalarExpr(const Expr *E);
  llvm::Value *EmitScalarUnaryMinus(llvm::Value *Val);
  llvm::Value *EmitScalarBinaryExpr(BinaryExpr::Operator Op,
                                    llvm::Value *LHS,
                                    llvm::Value *RHS);
  llvm::Value *EmitIntToInt32Conversion(llvm::Value *Value);
  llvm::Value *EmitSizeIntToIntConversion(llvm::Value *Value);
  llvm::Value *EmitScalarToScalarConversion(llvm::Value *Value, QualType Target);
  llvm::Value *EmitLogicalConditionExpr(const Expr *E);
  llvm::Value *EmitLogicalValueExpr(const Expr *E);
  llvm::Value *EmitIntegerConstantExpr(const IntegerConstantExpr *E);
  llvm::Value *EmitScalarRelationalExpr(BinaryExpr::Operator Op, llvm::Value *LHS,
                                        llvm::Value *RHS);
  llvm::Value *ConvertComparisonResultToRelationalOp(BinaryExpr::Operator Op,
                                                     llvm::Value *Result);

  llvm::Value *GetConstantZero(llvm::Type *T);
  llvm::Value *GetConstantZero(QualType T) {
    return GetConstantZero(ConvertType(T));
  }
  llvm::Value *GetConstantOne(QualType T);

  // complex expressions.
  ComplexValueTy ExtractComplexValue(llvm::Value *Agg);
  llvm::Value   *CreateComplexAggregate(ComplexValueTy Value);
  llvm::Value   *CreateComplexVector(ComplexValueTy Value);
  ComplexValueTy EmitComplexExpr(const Expr *E);
  ComplexValueTy EmitComplexLoad(llvm::Value *Ptr, bool IsVolatile = false);
  void EmitComplexStore(ComplexValueTy Value, llvm::Value *Ptr,
                        bool IsVolatile = false);
  ComplexValueTy EmitComplexBinaryExpr(BinaryExpr::Operator Op, ComplexValueTy LHS,
                                       ComplexValueTy RHS);
  ComplexValueTy EmitComplexPowi(ComplexValueTy LHS, llvm::Value *RHS);
  ComplexValueTy EmitComplexPow(ComplexValueTy LHS, ComplexValueTy RHS);
  ComplexValueTy EmitComplexToComplexConversion(ComplexValueTy Value, QualType Target);
  ComplexValueTy EmitScalarToComplexConversion(llvm::Value *Value, QualType Target);
  llvm::Value *EmitComplexToScalarConversion(ComplexValueTy Value, QualType Target);
  llvm::Value *EmitComplexRelationalExpr(BinaryExpr::Operator Op, ComplexValueTy LHS,
                                         ComplexValueTy RHS);

  // character expressesions
  CharacterValueTy ExtractCharacterValue(llvm::Value *Agg);
  llvm::Value   *CreateCharacterAggregate(CharacterValueTy Value);
  void EmitCharacterAssignment(const Expr *LHS, const Expr *RHS);
  llvm::Value *GetCharacterTypeLength(QualType T);
  CharacterValueTy GetCharacterValueFromAlloca(llvm::Value *Ptr,
                                               QualType StorageType);
  CharacterValueTy EmitCharacterExpr(const Expr *E);
  llvm::Value *EmitCharacterRelationalExpr(BinaryExpr::Operator Op, CharacterValueTy LHS,
                                           CharacterValueTy RHS);
  RValueTy EmitIntrinsicCallCharacter(intrinsic::FunctionKind Func,
                                      CharacterValueTy Value);
  RValueTy EmitIntrinsicCallCharacter(intrinsic::FunctionKind Func,
                                      CharacterValueTy A1, CharacterValueTy A2);
  llvm::Value *EmitCharacterDereference(CharacterValueTy Value);

  // intrinsic calls
  RValueTy EmitIntrinsicCall(const IntrinsicCallExpr *E);
  llvm::Value *EmitIntrinsicCallScalarTruncation(intrinsic::FunctionKind Func,
                                                 llvm::Value *Value,
                                                 QualType ResultType);
  RValueTy EmitIntrinsicCallComplex(intrinsic::FunctionKind Func, ComplexValueTy Value);
  llvm::Value *EmitIntrinsicCallScalarMath(intrinsic::FunctionKind Func,
                                           llvm::Value *A1, llvm::Value *A2 = nullptr);
  llvm::Value *EmitIntrinsicMinMax(intrinsic::FunctionKind Func,
                                   ArrayRef<Expr*> Arguments);
  llvm::Value *EmitIntrinsicScalarMinMax(intrinsic::FunctionKind Func,
                                         ArrayRef<llvm::Value*> Args);
  RValueTy EmitIntrinsicCallComplexMath(intrinsic::FunctionKind Func,
                                        ComplexValueTy Value);


  // calls
  RValueTy EmitCall(const CallExpr *E);
  RValueTy EmitCall(const FunctionDecl *Function,
                    CallArgList &ArgList,
                    ArrayRef<Expr *> Arguments,
                    bool ReturnsNothing = false);
  RValueTy EmitCall(llvm::Value *Callee,
                    const CGFunctionInfo *FuncInfo,
                    CallArgList &ArgList,
                    ArrayRef<Expr*> Arguments,
                    bool ReturnsNothing = false);
  RValueTy EmitCall(CGFunction Func,
                    ArrayRef<RValueTy> Arguments);

  RValueTy EmitCall1(CGFunction Func,
                     RValueTy Arg) {
    return EmitCall(Func, Arg);
  }

  RValueTy EmitCall2(CGFunction Func,
                     RValueTy A1, RValueTy A2) {
    RValueTy Args[] = { A1, A2 };
    return EmitCall(Func, llvm::makeArrayRef(Args, 2));
  }

  RValueTy EmitCall3(CGFunction Func,
                     RValueTy A1, RValueTy A2, RValueTy A3) {
    RValueTy Args[] = { A1, A2, A3 };
    return EmitCall(Func, llvm::makeArrayRef(Args, 3));
  }

  void EmitCallArg(llvm::SmallVectorImpl<llvm::Value*> &Args,
                   const Expr *E, CGFunctionInfo::ArgInfo ArgInfo);
  void EmitCallArg(llvm::SmallVectorImpl<llvm::Value*> &Args,
                   llvm::Value *Value, CGFunctionInfo::ArgInfo ArgInfo);
  void EmitCallArg(llvm::SmallVectorImpl<llvm::Value*> &Args,
                   ComplexValueTy Value, CGFunctionInfo::ArgInfo ArgInfo);
  void EmitCallArg(llvm::SmallVectorImpl<llvm::Value*> &Args,
                   CharacterValueTy Value, CGFunctionInfo::ArgInfo ArgInfo);
  llvm::Value *EmitCallArgPtr(const Expr *E);

  llvm::CallInst *EmitRuntimeCall(llvm::Value *Func);
  llvm::CallInst *EmitRuntimeCall(llvm::Value *Func, llvm::ArrayRef<llvm::Value*> Args);
  llvm::CallInst *EmitRuntimeCall2(llvm::Value *Func, llvm::Value *A1, llvm::Value *A2);

  // arrays
  RValueTy EmitArrayElementExpr(const ArrayElementExpr *E);

};

}  // end namespace CodeGen
}  // end namespace flang

#endif
