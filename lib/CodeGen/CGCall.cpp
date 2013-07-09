//===--- CGCall.cpp - Encapsulate calling convention details ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// These classes wrap the information about a call or function
// definition used to handle ABI compliancy.
//
//===----------------------------------------------------------------------===//

#include "CGCall.h"
#include "CodeGenFunction.h"
#include "CodeGenModule.h"
#include "flang/AST/Decl.h"
#include "flang/Frontend/CodeGenOptions.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Transforms/Utils/Local.h"

namespace flang {
namespace CodeGen {

RValueTy CodeGenFunction::EmitCall(const CallExpr *E) {
  return RValueTy();
}

}
} // end namespace flang
