//===-- FrontendAction.h - Generic Frontend Action Interface ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_FRONTEND_FRONTENDACTION_H
#define LLVM_FLANG_FRONTEND_FRONTENDACTION_H

#include "clang/Frontend/FrontendAction.h"

namespace flang {

typedef clang::FrontendAction FrontendAction;
typedef clang::ASTFrontendAction ASTFrontendAction;

}  // end namespace flang

#endif
