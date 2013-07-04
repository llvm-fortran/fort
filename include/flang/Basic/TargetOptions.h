//===--- TargetOptions.h ----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the flang::TargetOptions class.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_FLANG_FRONTEND_TARGETOPTIONS_H
#define LLVM_FLANG_FRONTEND_TARGETOPTIONS_H

#include "clang/Basic/TargetOptions.h"

namespace flang {

typedef clang::TargetOptions TargetOptions;

}  // end namespace flang

#endif
