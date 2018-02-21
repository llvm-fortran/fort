//===----- CGSystemRuntime.h - Interface to System specific Runtimes ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This provides an abstract class for system specific runtime calls.
//
//===----------------------------------------------------------------------===//

#ifndef FORT_CODEGEN_SYSTEMRUNTIME_H
#define FORT_CODEGEN_SYSTEMRUNTIME_H

#include "fort/AST/Stmt.h"

namespace llvm {
class Value;
class Type;
} // namespace llvm

namespace fort {
namespace CodeGen {

class CodeGenFunction;
class CodeGenModule;

class CGSystemRuntime {
protected:
  CodeGenModule &CGM;

public:
  CGSystemRuntime(CodeGenModule &cgm) : CGM(cgm) {}
  virtual ~CGSystemRuntime();

  virtual void EmitInit(CodeGenFunction &CGF) = 0;

  virtual llvm::Value *EmitMalloc(CodeGenFunction &CGF, llvm::Value *Size) = 0;
  llvm::Value *EmitMalloc(CodeGenFunction &CGF, llvm::Type *T,
                          llvm::Value *Size);
  virtual void EmitFree(CodeGenFunction &CGF, llvm::Value *Ptr) = 0;

  virtual llvm::Value *EmitETIME(CodeGenFunction &CGF,
                                 ArrayRef<Expr *> Arguments) = 0;
};

/// Creates an instance of a Libfort System runtime class.
CGSystemRuntime *CreateLibfortSystemRuntime(CodeGenModule &CGM);

} // namespace CodeGen
} // end namespace fort

#endif
