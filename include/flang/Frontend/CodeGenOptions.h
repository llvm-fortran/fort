//===--- CodeGenOptions.h ---------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the CodeGenOptions interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_FRONTEND_CODEGENOPTIONS_H
#define LLVM_FLANG_FRONTEND_CODEGENOPTIONS_H

#include "clang/Frontend/CodeGenOptions.h"

namespace flang {

typedef clang::CodeGenOptions CodeGenOptions;

}  // end namespace flang

#endif
