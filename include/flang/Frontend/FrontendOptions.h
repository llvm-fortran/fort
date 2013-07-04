//===--- FrontendOptions.h --------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_FRONTEND_FRONTENDOPTIONS_H
#define LLVM_FLANG_FRONTEND_FRONTENDOPTIONS_H

#include "clang/Frontend/FrontendOptions.h"

namespace flang {

using clang::frontend::ActionKind;
using clang::InputKind;

typedef clang::FrontendInputFile FrontendInputFile;
typedef clang::FrontendOptions FrontendOptions;

}  // end namespace flang

#endif
