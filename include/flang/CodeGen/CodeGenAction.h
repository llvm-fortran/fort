//===--- CodeGenAction.h - LLVM Code Generation Frontend Action -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_CODEGEN_CODE_GEN_ACTION_H
#define LLVM_FLANG_CODEGEN_CODE_GEN_ACTION_H

#include "clang/CodeGen/CodeGenAction.h"

namespace flang {

typedef clang::CodeGenAction CodeGenAction;
typedef clang::EmitAssemblyAction EmitAssemblyAction;
typedef clang::EmitBCAction EmitBCAction;
typedef clang::EmitLLVMAction EmitLLVMAction;
typedef clang::EmitLLVMOnlyAction EmitLLVMOnlyAction;
typedef clang::EmitCodeGenOnlyAction EmitCodeGenOnlyAction;
typedef clang::EmitObjAction EmitObjAction;

}

#endif
