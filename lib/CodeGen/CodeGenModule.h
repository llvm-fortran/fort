//===--- CodeGenModule.h - Per-Module state for LLVM CodeGen ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the internal per-translation-unit state used for llvm translation.
//
//===----------------------------------------------------------------------===//

#ifndef FLANG_CODEGEN_CODEGENMODULE_H
#define FLANG_CODEGEN_CODEGENMODULE_H

#include "CodeGenTypes.h"
#include "flang/AST/Decl.h"
#include "flang/Basic/LangOptions.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ValueHandle.h"
#include "llvm/Transforms/Utils/BlackList.h"

namespace llvm {
  class Module;
  class Constant;
  class ConstantInt;
  class Function;
  class GlobalValue;
  class DataLayout;
  class FunctionType;
  class LLVMContext;
}

namespace flang {
  class ASTContext;
  class Decl;
  class Expr;
  class Stmt;
  class NamedDecl;
  class ValueDecl;
  class VarDecl;
  class LangOptions;
  class DiagnosticsEngine;

namespace CodeGen {

struct CodeGenTypeCache {
  /// void
  llvm::Type *VoidTy;

  /// i8, i16, i32, and i64
  llvm::IntegerType *Int8Ty, *Int16Ty, *Int32Ty, *Int64Ty;
  /// float, double
  llvm::Type *FloatTy, *DoubleTy;

  /// int
  llvm::IntegerType *IntTy;

  /// intptr_t, size_t, and ptrdiff_t, which we assume are the same size.
  union {
    llvm::IntegerType *IntPtrTy;
    llvm::IntegerType *SizeTy;
    llvm::IntegerType *PtrDiffTy;
  };

  /// void* in address space 0
  union {
    llvm::PointerType *VoidPtrTy;
    llvm::PointerType *Int8PtrTy;
  };

  /// void** in address space 0
  union {
    llvm::PointerType *VoidPtrPtrTy;
    llvm::PointerType *Int8PtrPtrTy;
  };

  /// The width of a pointer into the generic address space.
  unsigned char PointerWidthInBits;

  /// The size and alignment of a pointer into the generic address
  /// space.
  union {
    unsigned char PointerAlignInBytes;
    unsigned char PointerSizeInBytes;
    unsigned char SizeSizeInBytes;     // sizeof(size_t)
  };

  llvm::CallingConv::ID RuntimeCC;
  llvm::CallingConv::ID getRuntimeCC() const {
    return RuntimeCC;
  }
};

/// CodeGenModule - This class organizes the cross-function state that is used
/// while generating LLVM code.
class CodeGenModule : public CodeGenTypeCache {
  CodeGenModule(const CodeGenModule &) LLVM_DELETED_FUNCTION;
  void operator=(const CodeGenModule &) LLVM_DELETED_FUNCTION;

  ASTContext &Context;
  const LangOptions &LangOpts;
  const CodeGenOptions &CodeGenOpts;
  llvm::Module &TheModule;
  DiagnosticsEngine &Diags;
  const llvm::DataLayout &TheDataLayout;
  //const TargetInfo &Target;
  llvm::LLVMContext &VMContext;

  CodeGenTypes Types;

public:
  CodeGenModule(ASTContext &C, const CodeGenOptions &CodeGenOpts,
                llvm::Module &M, const llvm::DataLayout &TD,
                DiagnosticsEngine &Diags);

  ~CodeGenModule();

  ASTContext &getContext() const { return Context; }

  llvm::Module &getModule() const { return TheModule; }

  llvm::LLVMContext &getLLVMContext() const { return VMContext; }

  CodeGenTypes &getTypes() { return Types; }

  /// Release - Finalize LLVM code generation.
  void Release();

  void EmitTopLevelDecl(const Decl *Declaration);

  void EmitMainProgramDecl(const MainProgramDecl *Program);

  void EmitFunctionDecl(const FunctionDecl *Function);

  llvm::Value* GetRuntimeFunction(StringRef Name,
                                  ArrayRef<llvm::Type*> ArgTypes,
                                  llvm::Type *ReturnType = nullptr);
};

}  // end namespace CodeGen
}  // end namespace flang

#endif
