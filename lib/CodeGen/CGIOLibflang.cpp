//===----- CGIOLibflang.cpp - Interface to Libflang IO Runtime -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This provides a class for IO statements code generation for the Libflang
// runtime library.
//
//===----------------------------------------------------------------------===//

#include "CGIORuntime.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"

namespace flang {
namespace CodeGen {

class CGLibflangIORuntime : public CGIORuntime {
public:
  CGLibflangIORuntime(CodeGenModule &CGM)
    : CGIORuntime(CGM) {}

  void EmitWriteStmt(const WriteStmt *S);
  void EmitPrintStmt(const PrintStmt *S);
};

void CGLibflangIORuntime::EmitWriteStmt(const WriteStmt *S) {
  // FIXME
}

void CGLibflangIORuntime::EmitPrintStmt(const PrintStmt *S) {
  // FIXME
}

CGIORuntime *CreateLibflangIORuntime(CodeGenModule &CGM) {
  return new CGLibflangIORuntime(CGM);
}

}
} // end namespace flang
